#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "consistent_hash/types.hpp"

namespace consistent_hash {

// Records lookup latencies and migration counts for observability.
class Metrics {
public:
    void record_lookup(std::chrono::nanoseconds duration) {
        std::lock_guard<std::mutex> lock(mutex_);
        const double ns =
            static_cast<double>(duration.count());
        if (latencies_.size() >= max_samples) {
            latencies_.erase(latencies_.begin());
        }
        latencies_.push_back(ns);
    }

    void record_migrations(uint64_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        migration_count_ += count;
    }

    uint64_t migration_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return migration_count_;
    }

    double avg_lookup_ns() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latencies_.empty()) return 0.0;
        double sum = 0.0;
        for (double v : latencies_) sum += v;
        return sum / static_cast<double>(latencies_.size());
    }

    double p99_lookup_ns() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latencies_.empty()) return 0.0;
        std::vector<double> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());
        const size_t idx =
            (sorted.size() * 99 + 99) / 100;  // ceil(p99)
        const size_t i = idx == 0 ? 0 : idx - 1;
        return sorted[i];
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        latencies_.clear();
        migration_count_ = 0;
    }

private:
    static constexpr size_t max_samples = 1000;
    mutable std::mutex mutex_;
    std::vector<double> latencies_;
    uint64_t migration_count_ = 0;
};

}  // namespace consistent_hash
