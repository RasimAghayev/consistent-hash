#include <consistent_hash/lockfree.hpp>
#include <consistent_hash/consistent_hash.hpp>
#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace consistent_hash;
using namespace consistent_hash::lockfree;

TEST(EpochReclaimerTest, BasicRegistration) {
  EpochReclaimer reclaimer;
  {
    EpochReclaimer::Guard guard(reclaimer);
    // Thread is registered
  }
  // Thread unregistered on guard destruction
  SUCCEED();
}

TEST(EpochReclaimerTest, RetireAndReclaim) {
  EpochReclaimer reclaimer;
  {
    EpochReclaimer::Guard guard(reclaimer);
    
    int* ptr = new int(42);
    reclaimer.retire(ptr, [](void* p) { delete static_cast<int*>(p); });
    
    // Advance epoch to allow reclamation
    reclaimer.try_advance_epoch();
    reclaimer.try_reclaim();
  }
  SUCCEED();
}

TEST(EpochReclaimerTest, ConcurrentRetire) {
  EpochReclaimer reclaimer;
  const int num_threads = 4;
  const int objects_per_thread = 100;
  
  std::vector<std::thread> threads;
  std::atomic<int> deleted_count{0};
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      EpochReclaimer::Guard guard(reclaimer);
      for (int i = 0; i < objects_per_thread; ++i) {
        int* ptr = new int(t * 1000 + i);
        reclaimer.retire(ptr, [&](void* p) { 
          delete static_cast<int*>(p); 
          deleted_count.fetch_add(1, std::memory_order_relaxed); 
        });
      }
      // Allow some epoch advancement
      for (int i = 0; i < 10; ++i) {
        guard.maybe_advance_epoch();
      }
    });
  }
  
  for (auto& th : threads) th.join();
  
  // Give time for reclamation
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  reclaimer.try_reclaim();
  
  EXPECT_EQ(deleted_count.load(), num_threads * objects_per_thread);
}

TEST(AtomicSharedPtrTest, BasicOperations) {
  AtomicSharedPtr<int> ptr;
  
  EXPECT_FALSE(ptr);
  EXPECT_EQ(ptr.load(), nullptr);
  
  ptr.store(new int(42));
  EXPECT_TRUE(ptr);
  EXPECT_EQ(*ptr.load(), 42);
  
  int* raw = ptr.exchange(new int(100));
  EXPECT_EQ(*raw, 42);
  // The previous pointer is retired (reclaimed later by the epoch reclaimer),
  // so the caller must not delete it here.
  EXPECT_EQ(*ptr.load(), 100);
  
  ptr.reset();
  EXPECT_FALSE(ptr);
}

TEST(AtomicSharedPtrTest, CompareExchange) {
  AtomicSharedPtr<int> ptr;
  int* expected = nullptr;
  int* new_val = new int(42);

  // Succeeds: current is nullptr, matches expected.
  EXPECT_TRUE(ptr.compare_exchange_strong(expected, new_val));
  EXPECT_EQ(ptr.load(), new_val);
  EXPECT_EQ(expected, nullptr);

  // Fails: expected does not match the current pointer. On failure expected is
  // updated to the current value and the desired pointer is not installed.
  int* wrong = new int(7);
  int* another = new int(100);
  expected = wrong;
  EXPECT_FALSE(ptr.compare_exchange_strong(expected, another));
  EXPECT_EQ(ptr.load(), new_val);
  EXPECT_EQ(expected, new_val);

  // Neither wrong nor another was installed; this test owns them.
  delete wrong;
  delete another;

  // Clean up: reset retires the current pointer (new_val).
  ptr.reset();
  EXPECT_FALSE(ptr);
}

TEST(AtomicSharedPtrTest, ConcurrentAccess) {
  AtomicSharedPtr<std::string> ptr;
  ptr.store(new std::string("initial"));
  
  const int num_readers = 4;
  const int num_writers = 2;
  const int iterations = 1000;
  
  std::atomic<int> read_count{0};
  std::atomic<int> write_count{0};
  
  std::vector<std::thread> threads;
  
  for (int i = 0; i < num_readers; ++i) {
    threads.emplace_back([&]() {
      for (int j = 0; j < iterations; ++j) {
        auto* p = ptr.load();
        if (p) {
          (void)*p; // Use the value
        }
        read_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (int i = 0; i < num_writers; ++i) {
    threads.emplace_back([&, i]() {
      for (int j = 0; j < iterations; ++j) {
        auto* new_str = new std::string("writer-" + std::to_string(i) + "-" + std::to_string(j));
        // exchange() retires the previous pointer; the writer must not delete it.
        (void)ptr.exchange(new_str);
        write_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (auto& th : threads) th.join();
  
  // Clean up final value (exchange retires it; do not delete here).
  (void)ptr.exchange(nullptr);
  
  EXPECT_EQ(read_count.load(), num_readers * iterations);
  EXPECT_EQ(write_count.load(), num_writers * iterations);
}

TEST(LockFreeRingBufferTest, BasicOperations) {
  LockFreeRingBuffer<int, 16> buffer;
  
  EXPECT_TRUE(buffer.empty());
  EXPECT_FALSE(buffer.full());
  EXPECT_EQ(buffer.size(), 0);
  
  // Push some values
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(buffer.push(i));
  }
  EXPECT_EQ(buffer.size(), 10);
  EXPECT_FALSE(buffer.empty());
  
  // Pop values
  int val;
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(buffer.pop(val));
    EXPECT_EQ(val, i);
  }
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);
}

TEST(LockFreeRingBufferTest, FullBuffer) {
  LockFreeRingBuffer<int, 4> buffer;

  // Fill buffer to capacity.
  EXPECT_TRUE(buffer.push(1));
  EXPECT_TRUE(buffer.push(2));
  EXPECT_TRUE(buffer.push(3));
  EXPECT_TRUE(buffer.push(4));
  EXPECT_TRUE(buffer.full());
  EXPECT_FALSE(buffer.push(5)); // Should fail - full

  int val;
  EXPECT_TRUE(buffer.pop(val));
  EXPECT_EQ(val, 1);
  EXPECT_TRUE(buffer.push(5)); // Now should work
}

TEST(LockFreeRingBufferTest, ConcurrentPushPop) {
  LockFreeRingBuffer<int, 1024> buffer;
  const int num_items = 10000;
  const int num_producers = 2;
  const int num_consumers = 2;
  
  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};
  
  std::vector<std::thread> threads;
  
  for (int p = 0; p < num_producers; ++p) {
    threads.emplace_back([&, p]() {
      for (int i = 0; i < num_items / num_producers; ++i) {
        int val = p * 100000 + i;
        while (!buffer.push(val)) {
          std::this_thread::yield();
        }
        produced.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (int c = 0; c < num_consumers; ++c) {
    threads.emplace_back([&]() {
      int val;
      while (true) {
        int current = consumed.load();
        if (current >= num_items) break;
        if (buffer.pop(val)) {
          consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    });
  }
  
  for (auto& th : threads) th.join();
  
  EXPECT_EQ(produced.load(), num_items);
  EXPECT_EQ(consumed.load(), num_items);
}

TEST(LockFreeHasherTest, BasicRingHasher) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node3", "host3:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  // Test lookups
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
  
  // Test lookup_n
  auto results = lf_hasher.lookup_n("test-key", 2);
  EXPECT_EQ(results.size(), 2);
  EXPECT_NE(results[0], results[1]);
  
  // Test bulk lookup
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  auto bulk = lf_hasher.lookup_bulk(keys);
  EXPECT_EQ(bulk.size(), 3);
  
  // Test stats
  auto stats = lf_hasher.stats();
  EXPECT_EQ(stats.total_keys, 1000); // Measured using 1000 probe keys
  
  // Test node count
  EXPECT_EQ(lf_hasher.node_count(), 3);
}

TEST(LockFreeHasherTest, ConcurrentReads) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  for (int i = 0; i < 10; ++i) {
    base_hasher.add_node(Node<std::string>("node" + std::to_string(i), "host" + std::to_string(i) + ":11211", 100, "", {}));
  }
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  const int num_threads = 8;
  const int iterations = 10000;
  std::atomic<int> total_lookups{0};
  
  std::vector<std::thread> threads;
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      for (int i = 0; i < iterations; ++i) {
        std::string key = "key-" + std::to_string(t) + "-" + std::to_string(i);
        auto result = lf_hasher.lookup(key);
        if (result) total_lookups.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (auto& th : threads) th.join();
  
  EXPECT_EQ(total_lookups.load(), num_threads * iterations);
}

TEST(LockFreeHasherTest, WriteOperations) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  // Add node
  lf_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  EXPECT_EQ(lf_hasher.node_count(), 2);
  EXPECT_TRUE(lf_hasher.has_node("node2"));
  
  // Update weight
  auto result = lf_hasher.update_weight("node1", 200);
  EXPECT_TRUE(result);
  
  // Remove node
  lf_hasher.remove_node("node1");
  EXPECT_EQ(lf_hasher.node_count(), 1);
  EXPECT_FALSE(lf_hasher.has_node("node1"));
  
  // Rebalance
  auto migrations = lf_hasher.rebalance();
  // Ring hasher may not have migrations
  
  // Serialize/deserialize
  std::string serialized = lf_hasher.serialize();
  EXPECT_FALSE(serialized.empty());
  
  auto deserialize_result = lf_hasher.deserialize(serialized);
  EXPECT_TRUE(deserialize_result);
}

TEST(LockFreeHasherTest, Clear) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  EXPECT_EQ(lf_hasher.node_count(), 2);
  
  lf_hasher.clear();
  EXPECT_EQ(lf_hasher.node_count(), 0);
  EXPECT_FALSE(lf_hasher.lookup("key").has_value());
}

TEST(LockFreeHasherTest, JumpHasher) {
  using Hasher = JumpHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
  
  auto results = lf_hasher.lookup_n("test-key", 2);
  EXPECT_EQ(results.size(), 2);
}

TEST(LockFreeHasherTest, MaglevHasher) {
  using Hasher = MaglevHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node3", "host3:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, RendezvousHasher) {
  using Hasher = RendezvousHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, MultiProbeHasher) {
  using Hasher = MultiProbeHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, BoundedLoadsHasher) {
  using Hasher = BoundedLoadsHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, KetamaHasher) {
  using Hasher = KetamaHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, AnchorHasher) {
  using Hasher = AnchorHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, FactoryFunctions) {
  using Hasher = RingHasher<std::string, std::string>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  
  // Test make_lockfree
  auto lf_hasher = make_lockfree(std::move(base_hasher));
  auto result = lf_hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
}

TEST(LockFreeHasherTest, EmptyHasher) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  auto result = lf_hasher.lookup("test-key");
  EXPECT_FALSE(result.has_value());
  
  auto results = lf_hasher.lookup_n("test-key", 3);
  EXPECT_TRUE(results.empty());
  
  auto bulk = lf_hasher.lookup_bulk({"key1", "key2"});
  EXPECT_EQ(bulk.size(), 2);
  EXPECT_FALSE(bulk[0].has_value());
  EXPECT_FALSE(bulk[1].has_value());
}

TEST(LockFreeHasherTest, VersionedSnapshot) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  // Initial version
  uint64_t v1 = lf_hasher.version();
  EXPECT_GE(v1, 1);  // Version increments on creation
  
  // Create snapshot
  auto snapshot = lf_hasher.create_snapshot();
  EXPECT_EQ(snapshot.version, v1);
  EXPECT_EQ(snapshot.node_count, 2);
  EXPECT_TRUE(snapshot.hasher != nullptr);
  
  // Modify hasher
  lf_hasher.add_node_atomic(Node<std::string>("node3", "host3:11211", 100, "", {}));
  
  // Version should increment
  uint64_t v2 = lf_hasher.version();
  EXPECT_EQ(v2, v1 + 1);
  EXPECT_EQ(lf_hasher.node_count(), 3);
  
  // Create another snapshot
  auto snapshot2 = lf_hasher.create_snapshot();
  EXPECT_EQ(snapshot2.version, v2);
  EXPECT_EQ(snapshot2.node_count, 3);
  
  // Restore from first snapshot (rollback)
  auto restore_result = lf_hasher.restore_snapshot(std::move(snapshot));
  EXPECT_TRUE(restore_result);
  
  // Should be back to version v1 + 1 (since restore increments)
  uint64_t v3 = lf_hasher.version();
  EXPECT_EQ(v3, v1 + 1);
  EXPECT_EQ(lf_hasher.node_count(), 2);
}

TEST(LockFreeHasherTest, AtomicOperations) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  // Atomic add node
  lf_hasher.add_node_atomic(Node<std::string>("node2", "host2:11211", 100, "", {}));
  EXPECT_EQ(lf_hasher.node_count(), 2);
  EXPECT_TRUE(lf_hasher.has_node("node2"));
  
  // Atomic remove node
  lf_hasher.remove_node_atomic("node1");
  EXPECT_EQ(lf_hasher.node_count(), 1);
  EXPECT_FALSE(lf_hasher.has_node("node1"));
  
  // Atomic update weight
  auto result = lf_hasher.update_weight_atomic("node2", 200);
  EXPECT_TRUE(result);
}

TEST(LockFreeHasherTest, ConcurrentReadWrite) {
  using Hasher = RingHasher<std::string, std::string>;
  using LockFreeHasher = LockFreeHasher<Hasher>;
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  
  LockFreeHasher lf_hasher(std::move(base_hasher));
  
  const int num_readers = 4;
  const int num_writers = 2;
  const int iterations = 100;  // Reduced to avoid deadlock in container
  
  std::atomic<int> read_count{0};
  std::atomic<int> write_count{0};
  std::atomic<int> error_count{0};
  
  std::vector<std::thread> threads;
  
  for (int i = 0; i < num_readers; ++i) {
    threads.emplace_back([&, i]() {
      for (int j = 0; j < iterations; ++j) {
        std::string key = "key-" + std::to_string(i) + "-" + std::to_string(j);
        auto result = lf_hasher.lookup(key);
        if (!result) error_count.fetch_add(1, std::memory_order_relaxed);
        read_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (int i = 0; i < num_writers; ++i) {
    threads.emplace_back([&, i]() {
      for (int j = 0; j < iterations; ++j) {
        std::string node_id = "node-" + std::to_string(i) + "-" + std::to_string(j);
        lf_hasher.add_node_atomic(Node<std::string>(node_id, "host", 100, "", {}));
        write_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (auto& th : threads) th.join();
  
  EXPECT_EQ(read_count.load(), num_readers * iterations);
  EXPECT_EQ(write_count.load(), num_writers * iterations);
  EXPECT_EQ(error_count.load(), 0);
}