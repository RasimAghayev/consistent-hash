#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "consistent_hash/types.hpp"

namespace consistent_hash {

// Percentile calculator using HDR histogram approximation.
class PercentileCalculator {
public:
  explicit PercentileCalculator(size_t max_samples = 10000)
      : max_samples_(max_samples) {}

  PercentileCalculator(const PercentileCalculator&) = delete;
  PercentileCalculator& operator=(const PercentileCalculator&) = delete;
  PercentileCalculator(PercentileCalculator&&) = delete;
  PercentileCalculator& operator=(PercentileCalculator&&) = delete;

  void record(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.size() >= max_samples_) {
      samples_.erase(samples_.begin());
    }
    samples_.push_back(value);
  }

  double percentile(double p) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty())
      return 0.0;
    std::vector<double> sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    const size_t idx = static_cast<size_t>(std::ceil(p * static_cast<double>(sorted.size()))) - 1;
    return sorted[std::min(idx, sorted.size() - 1)];
  }

  double mean() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty())
      return 0.0;
    double sum = 0.0;
    for (double v : samples_)
      sum += v;
    return sum / static_cast<double>(samples_.size());
  }

  size_t count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samples_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    samples_.clear();
  }

private:
  mutable std::mutex mutex_;
  std::vector<double> samples_;
  size_t max_samples_;
};

// Hash quality metrics for avalanche effect and collision analysis.
struct HashQualityMetrics {
  double avalanche_effect =
      0.0; // Fraction of bits flipped per input bit (ideal ~0.5)
  double collision_rate = 0.0; // Observed collision rate
  double bias_score = 0.0;     // Distribution bias (0 = uniform)
  size_t sample_count = 0;
  uint64_t total_collisions = 0;

  [[nodiscard]] bool good() const noexcept {
    return avalanche_effect > 0.45 && avalanche_effect < 0.55 &&
           collision_rate < 0.01;
  }
};

// Records lookup latencies, migration counts, and hash quality for
// observability.
class Metrics {
public:
  explicit Metrics(size_t max_samples = 10000)
      : latencies_(max_samples), hash_quality_samples_(1000) {}

  Metrics(const Metrics&) = delete;
  Metrics& operator=(const Metrics&) = delete;
  Metrics(Metrics&&) = delete;
  Metrics& operator=(Metrics&&) = delete;

  void record_lookup(std::chrono::nanoseconds duration) {
    latencies_.record(static_cast<double>(duration.count()));
  }

  void record_migrations(uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    migration_count_ += count;
  }

  // Record hash quality sample (for avalanche/collision analysis).
  void record_hash_quality(uint64_t hash1, uint64_t hash2, bool collision) {
    std::lock_guard<std::mutex> lock(quality_mutex_);
    if (hash_quality_samples_.size() >= 1000) {
      hash_quality_samples_.erase(hash_quality_samples_.begin());
    }
    hash_quality_samples_.push_back({hash1, hash2, collision});
  }

  uint64_t migration_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return migration_count_;
  }

  double avg_lookup_ns() const { return latencies_.mean(); }
  double p50_lookup_ns() const { return latencies_.percentile(0.50); }
  double p90_lookup_ns() const { return latencies_.percentile(0.90); }
  double p99_lookup_ns() const { return latencies_.percentile(0.99); }
  double p999_lookup_ns() const { return latencies_.percentile(0.999); }

  // Compute hash quality metrics from recorded samples.
  HashQualityMetrics compute_hash_quality() const {
    std::lock_guard<std::mutex> lock(quality_mutex_);
    HashQualityMetrics qm;
    qm.sample_count = hash_quality_samples_.size();
    if (qm.sample_count == 0)
      return qm;

    uint64_t collisions = 0;
    double total_bit_diff = 0.0;
    for (const auto &s : hash_quality_samples_) {
      if (s.collision)
        collisions++;
      // Compute Hamming distance (bits different) between hash1 and hash2
      uint64_t diff = s.hash1 ^ s.hash2;
      total_bit_diff += static_cast<double>(__builtin_popcountll(diff));
    }
    qm.total_collisions = collisions;
    qm.collision_rate = static_cast<double>(collisions) / static_cast<double>(qm.sample_count);
    // Avalanche effect: average fraction of 64 bits that differ per sample
    // Ideal is 32/64 = 0.5 (each input bit flip should flip ~50% of output
    // bits)
    qm.avalanche_effect = total_bit_diff / (64.0 * static_cast<double>(qm.sample_count));

    // Simple bias score: deviation from expected collision rate (1/2^64 for
    // 64-bit)
    const double expected_collision_rate =
        1.0 / (1ULL << 32); // Birthday bound approximation
    qm.bias_score = std::abs(qm.collision_rate - expected_collision_rate);

    return qm;
  }

  // Snapshot for reporting.
  struct Snapshot {
    uint64_t migration_count = 0;
    double avg_lookup_ns = 0.0;
    double p50_lookup_ns = 0.0;
    double p90_lookup_ns = 0.0;
    double p99_lookup_ns = 0.0;
    double p999_lookup_ns = 0.0;
    size_t sample_count = 0;
    HashQualityMetrics hash_quality;
  };

  Snapshot snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Snapshot{.migration_count = migration_count_,
                    .avg_lookup_ns = latencies_.mean(),
                    .p50_lookup_ns = latencies_.percentile(0.50),
                    .p90_lookup_ns = latencies_.percentile(0.90),
                    .p99_lookup_ns = latencies_.percentile(0.99),
                    .p999_lookup_ns = latencies_.percentile(0.999),
                    .sample_count = latencies_.count(),
                    .hash_quality = compute_hash_quality()};
  }

  void clear() {
    latencies_.clear();
    std::lock_guard<std::mutex> lock(mutex_);
    migration_count_ = 0;
    std::lock_guard<std::mutex> qlock(quality_mutex_);
    hash_quality_samples_.clear();
  }

private:
  struct QualitySample {
    uint64_t hash1;
    uint64_t hash2;
    bool collision;
  };

  mutable std::mutex mutex_;
  mutable std::mutex quality_mutex_;
  PercentileCalculator latencies_;
  std::vector<QualitySample> hash_quality_samples_;
  uint64_t migration_count_ = 0;
};

} // namespace consistent_hash