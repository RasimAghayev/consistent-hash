#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace consistent_hash::lockfree {

// Epoch-based reclamation for lock-free data structures.
//
// A reader registers a Guard, which pins the thread at the current global
// epoch for the duration of its critical section. Objects passed to retire()
// are stamped with the global epoch at retirement time and reclaimed only once
// every active thread has moved past that epoch (i.e. the object's epoch is
// strictly less than the minimum epoch among all active threads). The retired
// set is guarded by a mutex because reclamation is off the hot read path; the
// read-side pinning (Guard registration and epoch advancement) stays lock-free.
class EpochReclaimer {
public:
  static constexpr size_t MAX_THREADS = 128;

  EpochReclaimer() noexcept {
    global_epoch_.store(0, std::memory_order_relaxed);
    for (auto& slot : thread_slots_) {
      slot.epoch.store(0, std::memory_order_relaxed);
      slot.active.store(false, std::memory_order_relaxed);
    }
  }

  // Thread registration / critical-section guard.
  class Guard {
  public:
    explicit Guard(EpochReclaimer& reclaimer) noexcept : reclaimer_(&reclaimer) {
      register_thread();
    }
    ~Guard() noexcept { unregister_thread(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&& other) noexcept : reclaimer_(other.reclaimer_), index_(other.index_) {
      other.reclaimer_ = nullptr;
    }

    // Advance the global epoch and re-pin this thread at the new epoch.
    void maybe_advance_epoch() noexcept {
      if (!reclaimer_) return;
      uint64_t epoch = reclaimer_->global_epoch_.fetch_add(1, std::memory_order_acq_rel) + 1;
      if (index_ != SIZE_MAX) {
        reclaimer_->thread_slots_[index_].epoch.store(epoch, std::memory_order_release);
      }
    }

  private:
    void register_thread() noexcept {
      for (size_t i = 0; i < MAX_THREADS; ++i) {
        bool expected = false;
        if (reclaimer_->thread_slots_[i].active.compare_exchange_strong(
                expected, true, std::memory_order_acquire)) {
          index_ = i;
          reclaimer_->thread_slots_[i].epoch.store(
              reclaimer_->global_epoch_.load(std::memory_order_acquire),
              std::memory_order_release);
          return;
        }
      }
      // No slot available: fall back to the global epoch (no pinning).
      index_ = SIZE_MAX;
    }

    void unregister_thread() noexcept {
      if (reclaimer_ && index_ != SIZE_MAX) {
        reclaimer_->thread_slots_[index_].epoch.store(
            reclaimer_->global_epoch_.load(std::memory_order_acquire),
            std::memory_order_release);
        reclaimer_->thread_slots_[index_].active.store(false, std::memory_order_release);
      }
    }

    EpochReclaimer* reclaimer_ = nullptr;
    size_t index_ = SIZE_MAX;
  };

  // Retire an object for later reclamation.
  template <typename T, typename Deleter = std::default_delete<T>>
  void retire(T* ptr, Deleter deleter = {}) noexcept {
    if (!ptr) return;
    RetiredObject obj;
    obj.ptr = ptr;
    obj.deleter = [ptr, deleter]() { deleter(static_cast<T*>(ptr)); };
    obj.epoch = global_epoch_.load(std::memory_order_acquire);
    {
      std::lock_guard<std::mutex> lock(retired_mutex_);
      retired_.push_back(std::move(obj));
    }
    try_reclaim();
  }

  // Reclaim every retired object whose epoch predates all active threads.
  void try_reclaim() noexcept {
    uint64_t safe_epoch = global_epoch_.load(std::memory_order_acquire);
    for (size_t i = 0; i < MAX_THREADS; ++i) {
      if (thread_slots_[i].active.load(std::memory_order_acquire)) {
        uint64_t thread_epoch = thread_slots_[i].epoch.load(std::memory_order_acquire);
        if (thread_epoch < safe_epoch) {
          safe_epoch = thread_epoch;
        }
      }
    }

    std::lock_guard<std::mutex> lock(retired_mutex_);
    size_t kept = 0;
    for (size_t i = 0; i < retired_.size(); ++i) {
      if (retired_[i].epoch < safe_epoch && retired_[i].deleter) {
        retired_[i].deleter();
      } else {
        if (kept != i) {
          retired_[kept] = std::move(retired_[i]);
        }
        ++kept;
      }
    }
    retired_.resize(kept);
  }

  // Force advance the global epoch.
  void try_advance_epoch() noexcept {
    global_epoch_.fetch_add(1, std::memory_order_acq_rel);
    try_reclaim();
  }

private:
  struct ThreadSlot {
    std::atomic<uint64_t> epoch;
    std::atomic<bool> active;
  };

  struct RetiredObject {
    void* ptr = nullptr;
    std::function<void()> deleter;
    uint64_t epoch = 0;
  };

  alignas(64) std::atomic<uint64_t> global_epoch_;
  alignas(64) ThreadSlot thread_slots_[MAX_THREADS];
  std::mutex retired_mutex_;
  std::vector<RetiredObject> retired_;
};

// Global epoch reclaimer instance
inline EpochReclaimer& global_epoch_reclaimer() noexcept {
  static EpochReclaimer instance;
  return instance;
}

// Lock-free atomic shared pointer with epoch reclamation
template <typename T>
class AtomicSharedPtr {
public:
  using element_type = T;
  using pointer = T*;

  AtomicSharedPtr() noexcept = default;
  explicit AtomicSharedPtr(T* ptr) noexcept : ptr_(ptr) {}
  AtomicSharedPtr(std::shared_ptr<T> ptr) noexcept : ptr_(ptr.release()) {}
  ~AtomicSharedPtr() noexcept { reset(); }

  AtomicSharedPtr(const AtomicSharedPtr&) = delete;
  AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

  AtomicSharedPtr(AtomicSharedPtr&& other) noexcept : ptr_(other.ptr_.exchange(nullptr)) {}
  AtomicSharedPtr& operator=(AtomicSharedPtr&& other) noexcept {
    reset();
    ptr_.store(other.ptr_.exchange(nullptr), std::memory_order_relaxed);
    return *this;
  }

  // Lock-free load
  T* load(std::memory_order order = std::memory_order_acquire) const noexcept {
    return ptr_.load(order);
  }

  // Lock-free store with epoch reclamation
  void store(T* ptr, std::memory_order order = std::memory_order_release) noexcept {
    T* old = ptr_.exchange(ptr, order);
    if (old) {
      global_epoch_reclaimer().retire(old);
    }
  }

  // Lock-free compare-exchange
  bool compare_exchange_weak(T*& expected, T* desired,
                             std::memory_order success = std::memory_order_acq_rel,
                             std::memory_order failure = std::memory_order_acquire) noexcept {
    T* old = expected;
    if (ptr_.compare_exchange_weak(expected, desired, success, failure)) {
      if (old) {
        global_epoch_reclaimer().retire(old);
      }
      return true;
    }
    return false;
  }

  bool compare_exchange_strong(T*& expected, T* desired,
                               std::memory_order success = std::memory_order_acq_rel,
                               std::memory_order failure = std::memory_order_acquire) noexcept {
    T* old = expected;
    if (ptr_.compare_exchange_strong(expected, desired, success, failure)) {
      if (old) {
        global_epoch_reclaimer().retire(old);
      }
      return true;
    }
    return false;
  }

  // Exchange
  T* exchange(T* ptr, std::memory_order order = std::memory_order_acq_rel) noexcept {
    T* old = ptr_.exchange(ptr, order);
    if (old) {
      global_epoch_reclaimer().retire(old);
    }
    return old;
  }

  // Reset
  void reset() noexcept {
    T* old = ptr_.exchange(nullptr, std::memory_order_acq_rel);
    if (old) {
      global_epoch_reclaimer().retire(old);
    }
  }

  T* get() const noexcept { return ptr_.load(std::memory_order_relaxed); }
  explicit operator bool() const noexcept { return get() != nullptr; }

  T& operator*() const noexcept { return *get(); }
  T* operator->() const noexcept { return get(); }

private:
  std::atomic<T*> ptr_{nullptr};
};

// Lock-free read-optimized consistent hasher wrapper
template <typename Hasher>
class LockFreeHasher {
public:
  using node_id_type = typename Hasher::node_id_type;
  using key_type = typename Hasher::key_type;
  using hasher_type = Hasher;

  LockFreeHasher() = default;
  explicit LockFreeHasher(Hasher hasher) : hasher_(new Hasher(std::move(hasher))) {}
  
  explicit LockFreeHasher(std::unique_ptr<Hasher> hasher) : hasher_(hasher.release()) {}

  // Lock-free lookup (read-only, no locks)
  std::optional<node_id_type> lookup(const key_type& key) const noexcept {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) return std::nullopt;
    auto result = hasher_ptr->lookup(key);
    guard.maybe_advance_epoch();
    return result;
  }

  std::vector<node_id_type> lookup_n(const key_type& key, size_t n) const noexcept {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) return {};
    auto result = hasher_ptr->lookup_n(key, n);
    guard.maybe_advance_epoch();
    return result;
  }

  std::vector<std::optional<node_id_type>> lookup_bulk(
      const std::vector<key_type>& keys) const noexcept {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) {
      return std::vector<std::optional<node_id_type>>(keys.size());
    }
    auto result = hasher_ptr->lookup_bulk(keys);
    guard.maybe_advance_epoch();
    return result;
  }

  // Mutating operations (require exclusive access via versioned swap)
  void add_node(const typename Hasher::NodeType& node) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    new_hasher->add_node(node);
    swap_hasher(std::move(new_hasher));
  }

  bool upsert_node(const typename Hasher::NodeType& node) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    bool result = new_hasher->upsert_node(node);
    swap_hasher(std::move(new_hasher));
    return result;
  }

  void remove_node(const node_id_type& id) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    new_hasher->remove_node(id);
    swap_hasher(std::move(new_hasher));
  }

  typename Hasher::Result update_weight(const node_id_type& id, uint32_t weight) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    auto result = new_hasher->update_weight(id, weight);
    swap_hasher(std::move(new_hasher));
    return result;
  }

  std::vector<typename Hasher::MigrationType> rebalance() {
    auto new_hasher = std::make_unique<Hasher>(*current());
    auto result = new_hasher->rebalance();
    swap_hasher(std::move(new_hasher));
    return result;
  }

  typename Hasher::StatsType stats() const noexcept {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) return {};
    auto result = hasher_ptr->stats();
    guard.maybe_advance_epoch();
    return result;
  }

  size_t node_count() const noexcept {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) return 0;
    auto result = hasher_ptr->node_count();
    guard.maybe_advance_epoch();
    return result;
  }

  void clear() noexcept {
    swap_hasher(std::make_unique<Hasher>());
  }

  bool has_node(const node_id_type& id) const noexcept {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) return false;
    auto result = hasher_ptr->has_node(id);
    guard.maybe_advance_epoch();
    return result;
  }

  std::string serialize() const {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) return "";
    auto result = hasher_ptr->serialize();
    guard.maybe_advance_epoch();
    return result;
  }

  typename Hasher::Result deserialize(std::string_view data) {
    auto new_hasher = std::make_unique<Hasher>();
    auto result = new_hasher->deserialize(data);
    if (result) {
      swap_hasher(std::move(new_hasher));
    }
    return result;
  }

  // Versioned snapshot for dynamic reconfiguration
  struct Snapshot {
    std::unique_ptr<Hasher> hasher;
    uint64_t version;
    std::chrono::steady_clock::time_point timestamp;
    size_t node_count;
  };

  // Get current version
  uint64_t version() const noexcept {
    return version_.load(std::memory_order_acquire);
  }

  // Create a snapshot of current state
  Snapshot create_snapshot() const {
    EpochReclaimer::Guard guard(global_epoch_reclaimer());
    auto* hasher_ptr = hasher_.load();
    if (!hasher_ptr) {
      return Snapshot{nullptr, version_.load(), std::chrono::steady_clock::now(), 0};
    }
    auto snapshot_hasher = std::make_unique<Hasher>(*hasher_ptr);
    guard.maybe_advance_epoch();
    return Snapshot{
      std::move(snapshot_hasher),
      version_.load(),
      std::chrono::steady_clock::now(),
      hasher_ptr->node_count()
    };
  }

  // Restore from snapshot (for rollback)
  typename Hasher::Result restore_snapshot(Snapshot&& snapshot) {
    if (!snapshot.hasher) {
      return typename Hasher::Result{Hasher::ErrorCode::InvalidArgument, "Empty snapshot"};
    }
    swap_hasher(std::move(snapshot.hasher));
    version_.store(snapshot.version + 1, std::memory_order_release);
    return typename Hasher::Result{};
  }

  // Atomic add node with version increment
  void add_node_atomic(const typename Hasher::NodeType& node) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    new_hasher->add_node(node);
    swap_hasher(std::move(new_hasher));
    version_.fetch_add(1, std::memory_order_acq_rel);
  }

  // Atomic remove node with version increment
  void remove_node_atomic(const node_id_type& id) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    new_hasher->remove_node(id);
    swap_hasher(std::move(new_hasher));
    version_.fetch_add(1, std::memory_order_acq_rel);
  }

  // Atomic update weight with version increment
  typename Hasher::Result update_weight_atomic(const node_id_type& id, uint32_t weight) {
    auto new_hasher = std::make_unique<Hasher>(*current());
    auto result = new_hasher->update_weight(id, weight);
    if (result) {
      swap_hasher(std::move(new_hasher));
      version_.fetch_add(1, std::memory_order_acq_rel);
    }
    return result;
  }

private:
  Hasher* current() const noexcept {
    return hasher_.load(std::memory_order_relaxed);
  }

  void swap_hasher(std::unique_ptr<Hasher> new_hasher) {
    std::unique_lock lock(mutex_);
    hasher_.store(new_hasher.release(), std::memory_order_release);
  }

  mutable std::shared_mutex mutex_;  // Only for writers
  AtomicSharedPtr<Hasher> hasher_;
  std::atomic<uint64_t> version_{1};
};

// Lock-free MPMC ring buffer (Vyukov bounded queue) for metrics collection.
// Supports multiple concurrent producers and consumers. Holds up to Capacity
// elements; full()/empty()/size() are approximate snapshots.
template <typename T, size_t Capacity>
class LockFreeRingBuffer {
public:
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
  static_assert(Capacity >= 2, "Capacity must be at least 2");

  LockFreeRingBuffer() noexcept {
    for (size_t i = 0; i < Capacity; ++i) {
      buffer_[i].seq.store(i, std::memory_order_relaxed);
    }
  }

  LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
  LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

  bool push(const T& value) noexcept {
    size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
    for (;;) {
      Cell& cell = buffer_[pos & (Capacity - 1)];
      size_t seq = cell.seq.load(std::memory_order_acquire);
      intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
      if (diff == 0) {
        if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          cell.data = value;
          cell.seq.store(pos + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;  // Full
      } else {
        pos = enqueue_pos_.load(std::memory_order_relaxed);
      }
    }
  }

  bool pop(T& value) noexcept {
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    for (;;) {
      Cell& cell = buffer_[pos & (Capacity - 1)];
      size_t seq = cell.seq.load(std::memory_order_acquire);
      intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
      if (diff == 0) {
        if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          value = std::move(cell.data);
          cell.seq.store(pos + Capacity, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;  // Empty
      } else {
        pos = dequeue_pos_.load(std::memory_order_relaxed);
      }
    }
  }

  size_t size() const noexcept {
    size_t enq = enqueue_pos_.load(std::memory_order_acquire);
    size_t deq = dequeue_pos_.load(std::memory_order_acquire);
    return enq - deq;
  }

  bool empty() const noexcept {
    return enqueue_pos_.load(std::memory_order_acquire) ==
           dequeue_pos_.load(std::memory_order_acquire);
  }

  bool full() const noexcept { return size() >= Capacity; }

private:
  struct Cell {
    std::atomic<size_t> seq;
    T data;
  };

  alignas(64) std::atomic<size_t> enqueue_pos_{0};
  alignas(64) std::atomic<size_t> dequeue_pos_{0};
  alignas(64) std::array<Cell, Capacity> buffer_;
};

} // namespace consistent_hash::lockfree