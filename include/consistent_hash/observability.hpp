#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "consistent_hash/metrics.hpp"
#include "consistent_hash/types.hpp"

namespace consistent_hash {

// Forward declarations for optional OpenTelemetry integration
namespace opentelemetry {
namespace metrics {
class Meter;
class Counter;
class Histogram;
class ObservableGauge;
} // namespace metrics
} // namespace opentelemetry

// Prometheus metrics exporter interface
class PrometheusMetricsExporter {
public:
  struct MetricFamily {
    std::string name;
    std::string help;
    std::string type; // "counter", "gauge", "histogram", "summary"
    
    struct Sample {
      std::unordered_map<std::string, std::string> labels;
      double value;
      uint64_t timestamp_ms;
    };
    
    std::vector<Sample> samples;
  };
  
  virtual ~PrometheusMetricsExporter() = default;
  
  // Export all metrics in Prometheus text format
  virtual std::string export_metrics() const = 0;
  
  // Export specific metric families
  virtual std::vector<MetricFamily> collect() const = 0;
};

// OpenTelemetry metrics exporter interface
class OpenTelemetryMetricsExporter {
public:
  virtual ~OpenTelemetryMetricsExporter() = default;
  
  // Initialize with a meter provider
  virtual bool initialize(std::string_view service_name) = 0;
  
  // Record a lookup latency
  virtual void record_lookup_latency(std::chrono::nanoseconds duration,
                                     std::string_view algorithm,
                                     std::string_view node_id) = 0;
  
  // Record a migration event
  virtual void record_migration(std::string_view algorithm,
                                std::string_view from_node,
                                std::string_view to_node) = 0;
  
  // Record hash quality metrics
  virtual void record_hash_quality(std::string_view algorithm,
                                   const HashQualityMetrics& quality) = 0;
  
  // Record distribution stats
  virtual void record_distribution(std::string_view algorithm,
                                   const DistributionStats& stats) = 0;
  
  // Shutdown and flush
  virtual void shutdown() = 0;
};

// No-op implementations (used when OpenTelemetry/Prometheus not available)
class NoOpPrometheusExporter : public PrometheusMetricsExporter {
public:
  std::string export_metrics() const override { return ""; }
  std::vector<MetricFamily> collect() const override { return {}; }
};

class NoOpOpenTelemetryExporter : public OpenTelemetryMetricsExporter {
public:
  bool initialize(std::string_view) override { return false; }
  void record_lookup_latency(std::chrono::nanoseconds,
                             std::string_view,
                             std::string_view) override {}
  void record_migration(std::string_view, std::string_view, std::string_view) override {}
  void record_hash_quality(std::string_view, const HashQualityMetrics&) override {}
  void record_distribution(std::string_view, const DistributionStats&) override {}
  void shutdown() override {}
};

// Metrics registry for managing multiple exporters
class MetricsRegistry {
public:
  static MetricsRegistry& instance() {
    static MetricsRegistry registry;
    return registry;
  }
  
  void set_prometheus_exporter(std::unique_ptr<PrometheusMetricsExporter> exporter) {
    prometheus_exporter_ = std::move(exporter);
  }
  
  void set_opentelemetry_exporter(std::unique_ptr<OpenTelemetryMetricsExporter> exporter) {
    opentelemetry_exporter_ = std::move(exporter);
  }
  
  PrometheusMetricsExporter* prometheus_exporter() const {
    return prometheus_exporter_.get();
  }
  
  OpenTelemetryMetricsExporter* opentelemetry_exporter() const {
    return opentelemetry_exporter_.get();
  }
  
  // Export Prometheus metrics in text format
  std::string export_prometheus() const {
    if (prometheus_exporter_) {
      return prometheus_exporter_->export_metrics();
    }
    return "";
  }
  
  // Initialize OpenTelemetry with service name
  bool init_opentelemetry(std::string_view service_name) {
    if (opentelemetry_exporter_) {
      return opentelemetry_exporter_->initialize(service_name);
    }
    return false;
  }
  
  // Shutdown all exporters
  void shutdown() {
    if (opentelemetry_exporter_) {
      opentelemetry_exporter_->shutdown();
    }
  }
  
private:
  MetricsRegistry() = default;
  std::unique_ptr<PrometheusMetricsExporter> prometheus_exporter_;
  std::unique_ptr<OpenTelemetryMetricsExporter> opentelemetry_exporter_;
};

// Built-in Prometheus exporter that collects from Metrics objects
class BuiltinPrometheusExporter : public PrometheusMetricsExporter {
public:
  explicit BuiltinPrometheusExporter(std::string_view namespace_ = "consistent_hash")
      : namespace_(namespace_) {}
  
  // Register a metrics source with labels
  void register_metrics(std::string_view name,
                        const Metrics* metrics,
                        std::unordered_map<std::string, std::string> labels = {}) {
    std::lock_guard<std::mutex> lock(mutex_);
    sources_.emplace_back(Source{
      .name = std::string(name),
      .metrics = metrics,
      .labels = std::move(labels)
    });
  }
  
  void unregister_metrics(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    sources_.erase(
      std::remove_if(sources_.begin(), sources_.end(),
                     [&](const Source& s) { return s.name == name; }),
      sources_.end()
    );
  }
  
  std::string export_metrics() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string output;
    
    for (const auto& source : sources_) {
      if (!source.metrics) continue;
      
      const auto snapshot = source.metrics->snapshot();
      
      // Lookup latency histogram
      output += format_histogram(
        namespace_ + "_lookup_latency_nanoseconds",
        "Lookup latency in nanoseconds",
        {{"le", "50"}, {"le", "90"}, {"le", "99"}, {"le", "999"}, {"le", "+Inf"}},
        {snapshot.p50_lookup_ns, snapshot.p90_lookup_ns, snapshot.p99_lookup_ns,
         snapshot.p999_lookup_ns, snapshot.avg_lookup_ns * 10},
        source.labels
      );
      
      // Migration counter
      output += format_counter(
        namespace_ + "_migrations_total",
        "Total number of node migrations",
        snapshot.migration_count,
        source.labels
      );
      
      // Hash quality gauge
      const auto& hq = snapshot.hash_quality;
      output += format_gauge(
        namespace_ + "_hash_avalanche_effect",
        "Hash function avalanche effect (ideal ~0.5)",
        hq.avalanche_effect,
        source.labels
      );
      output += format_gauge(
        namespace_ + "_hash_collision_rate",
        "Observed hash collision rate",
        hq.collision_rate,
        source.labels
      );
      output += format_gauge(
        namespace_ + "_hash_bias_score",
        "Hash distribution bias score",
        hq.bias_score,
        source.labels
      );
    }
    
    return output;
  }
  
  std::vector<MetricFamily> collect() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MetricFamily> families;
    
    for (const auto& source : sources_) {
      if (!source.metrics) continue;
      
      const auto snapshot = source.metrics->snapshot();
      
      MetricFamily latency_family;
      latency_family.name = namespace_ + "_lookup_latency_nanoseconds";
      latency_family.help = "Lookup latency in nanoseconds";
      latency_family.type = "histogram";
      // Add buckets
      families.push_back(std::move(latency_family));
      
      MetricFamily migration_family;
      migration_family.name = namespace_ + "_migrations_total";
      migration_family.help = "Total number of node migrations";
      migration_family.type = "counter";
      migration_family.samples.push_back({source.labels, static_cast<double>(snapshot.migration_count), 
                                          current_timestamp_ms()});
      families.push_back(std::move(migration_family));
    }
    
    return families;
  }
  
private:
  struct Source {
    std::string name;
    const Metrics* metrics = nullptr;
    std::unordered_map<std::string, std::string> labels;
  };
  
  mutable std::mutex mutex_;
  std::vector<Source> sources_;
  std::string namespace_;
  
  static uint64_t current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  
  static std::string format_histogram(std::string_view name,
                                      std::string_view help,
                                      const std::vector<std::string_view>& le_labels,
                                      const std::vector<double>& values,
                                      const std::unordered_map<std::string, std::string>& labels) {
    std::string output;
    output += "# HELP " + std::string(name) + " " + std::string(help) + "\n";
    output += "# TYPE " + std::string(name) + " histogram\n";
    
    std::string base_labels = format_labels(labels);
    for (size_t i = 0; i < le_labels.size(); ++i) {
      std::string sample_labels = base_labels;
      if (!sample_labels.empty()) sample_labels += ",";
      sample_labels += "le=\"" + std::string(le_labels[i]) + "\"";
      output += std::string(name) + "{" + sample_labels + "} " + std::to_string(values[i]) + "\n";
    }
    output += "\n";
    return output;
  }
  
  static std::string format_counter(std::string_view name,
                                    std::string_view help,
                                    double value,
                                    const std::unordered_map<std::string, std::string>& labels) {
    std::string output;
    output += "# HELP " + std::string(name) + " " + std::string(help) + "\n";
    output += "# TYPE " + std::string(name) + " counter\n";
    output += std::string(name) + "{" + format_labels(labels) + "} " + std::to_string(value) + "\n\n";
    return output;
  }
  
  static std::string format_gauge(std::string_view name,
                                  std::string_view help,
                                  double value,
                                  const std::unordered_map<std::string, std::string>& labels) {
    std::string output;
    output += "# HELP " + std::string(name) + " " + std::string(help) + "\n";
    output += "# TYPE " + std::string(name) + " gauge\n";
    output += std::string(name) + "{" + format_labels(labels) + "} " + std::to_string(value) + "\n\n";
    return output;
  }
  
  static std::string format_labels(const std::unordered_map<std::string, std::string>& labels) {
    std::string result;
    for (const auto& [k, v] : labels) {
      if (!result.empty()) result += ",";
      result += k + "=\"" + v + "\"";
    }
    return result;
  }
};

// OpenTelemetry exporter implementation (requires opentelemetry-cpp)
#ifdef CONSISTENT_HASH_WITH_OPENTELEMETRY
class OpenTelemetryExporter : public OpenTelemetryMetricsExporter {
public:
  bool initialize(std::string_view service_name) override {
    // Initialize OpenTelemetry SDK
    // This is a placeholder - actual implementation would use opentelemetry-cpp
    service_name_ = service_name;
    initialized_ = true;
    return true;
  }
  
  void record_lookup_latency(std::chrono::nanoseconds duration,
                             std::string_view algorithm,
                             std::string_view node_id) override {
    if (!initialized_) return;
    // Record to OpenTelemetry histogram
  }
  
  void record_migration(std::string_view algorithm,
                        std::string_view from_node,
                        std::string_view to_node) override {
    if (!initialized_) return;
    // Record to OpenTelemetry counter
  }
  
  void record_hash_quality(std::string_view algorithm,
                           const HashQualityMetrics& quality) override {
    if (!initialized_) return;
    // Record to OpenTelemetry gauges
  }
  
  void record_distribution(std::string_view algorithm,
                           const DistributionStats& stats) override {
    if (!initialized_) return;
    // Record to OpenTelemetry gauges
  }
  
  void shutdown() override {
    initialized_ = false;
  }
  
private:
  bool initialized_ = false;
  std::string service_name_;
};
#endif

} // namespace consistent_hash