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

    void record(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.size() >= max_samples_) {
            samples_.erase(samples_.begin());
        }
        samples_.push_back(value);
    }

    double percentile(double p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        std::vector<double> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        const size_t idx = static_cast<size_t>(std::ceil(p * sorted.size())) - 1;
        return sorted[std::min(idx, sorted.size() - 1)];
    }

    double mean() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        double sum = 0.0;
        for (double v : samples_) sum += v;
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

// Records lookup latencies and migration counts for observability.
class Metrics {
public:
    explicit Metrics(size_t max_samples = 10000)
        : latencies_(max_samples) {}

    void record_lookup(std::chrono::nanoseconds duration) {
        latencies_.record(static_cast<double>(duration.count()));
    }

    void record_migrations(uint64_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        migration_count_ += count;
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

    // Snapshot for reporting.
    struct Snapshot {
        uint64_t migration_count = 0;
        double avg_lookup_ns = 0.0;
        double p50_lookup_ns = 0.0;
        double p90_lookup_ns = 0.0;
        double p99_lookup_ns = 0.0;
        double p999_lookup_ns = 0.0;
        size_t sample_count = 0;
    };

    Snapshot snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return Snapshot{
            .migration_count = migration_count_,
            .avg_lookup_ns = latencies_.mean(),
            .p50_lookup_ns = latencies_.percentile(0.50),
            .p90_lookup_ns = latencies_.percentile(0.90),
            .p99_lookup_ns = latencies_.percentile(0.99),
            .p999_lookup_ns = latencies_.percentile(0.999),
            .sample_count = latencies_.count()
        };
    }

    void clear() {
        latencies_.clear();
        std::lock_guard<std::mutex> lock(mutex_);
        migration_count_ = 0;
    }

private:
    mutable std::mutex mutex_;
    PercentileCalculator latencies_;
    uint64_t migration_count_ = 0;
};

}  // namespace consistent_hash