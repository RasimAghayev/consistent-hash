// Consistent Hash Rust Bindings
// This provides a Rust wrapper around the C++ consistent_hash library using cxx

// [package]
// name = "consistent-hash"
// version = "1.0.0"
// edition = "2021"
// description = "Modern C++20 Consistent Hashing Library - Rust Bindings"
// license = "MIT"
// repository = "https://github.com/consistent-hash/consistent-hash"
//
// [dependencies]
// cxx = "1.0"
// cxx-build = "1.0"
// thiserror = "1.0"
// serde = { version = "1.0", features = ["derive"] }
// serde_json = "1.0"
// parking_lot = "0.12"
// once_cell = "1.17"
//
// [build-dependencies]
// cxx-build = "1.0"
//
// [features]
// default = ["std"]
// std = []

// lib.rs
mod ffi;
mod hasher;
mod node;
mod error;
mod health;
mod simd;

pub use hasher::{Hasher, RingHasher, JumpHasher, MaglevHasher, RendezvousHasher, MultiProbeHasher, BoundedLoadsHasher, KetamaHasher, AnchorHasher, LockFreeHasher, HealthAwareHasher};
pub use node::{Node, Migration, DistributionStats};
pub use error::{ConsistentHashError, Result};
pub use health::{HealthChecker, HealthCheckConfig, HealthStatus, NodeHealth};
pub use simd::{AdaptiveHasher, SIMDType};

// ffi.rs
#[cxx::bridge]
mod ffi {
    // Node structure
    struct Node {
        id: String,
        address: String,
        weight: u32,
        zone: String,
        metadata: std::collections::HashMap<String, String>,
    }

    // Migration structure
    struct Migration {
        from: String,
        to: String,
        key: String,
    }

    // Distribution stats
    struct DistributionStats {
        total_keys: usize,
        unique_nodes_used: usize,
        mean_keys_per_node: f64,
        stddev_keys_per_node: f64,
        skew: f64,
        max_load: f64,
        min_load: f64,
        migration_count: u64,
        avg_lookup_ns: f64,
        p50_lookup_ns: f64,
        p90_lookup_ns: f64,
        p99_lookup_ns: f64,
        p999_lookup_ns: f64,
    }

    // Health status
    enum HealthStatus {
        Unknown,
        Healthy,
        Degraded,
        Unhealthy,
        Draining,
    }

    // Node health
    struct NodeHealth {
        node_id: String,
        status: HealthStatus,
        last_check: i64,
        last_status_change: i64,
        consecutive_failures: usize,
        consecutive_successes: usize,
        total_checks: u64,
        total_failures: u64,
        total_passive_errors: u64,
        last_passive_error: i64,
        last_error_message: String,
        latency_ms: f64,
    }

    // Health check config
    struct HealthCheckConfig {
        interval_ms: u64,
        timeout_ms: u64,
        failure_threshold: usize,
        success_threshold: usize,
        initial_delay_ms: u64,
        passive_checks: bool,
        passive_error_threshold: usize,
        passive_window_secs: u64,
    }

    // SIMD type
    enum SIMDType {
        HighwayHash,
        XxHash3,
        Fnv1a,
    }

    // Base hasher interface
    extern "Rust" {
        trait Hasher {
            fn add_node(&mut self, node: Node);
            fn upsert_node(&mut self, node: Node) -> bool;
            fn remove_node(&mut self, id: &str);
            fn update_weight(&mut self, id: &str, weight: u32) -> bool;
            fn has_node(&self, id: &str) -> bool;
            fn lookup(&self, key: &str) -> Option<String>;
            fn lookup_or_throw(&self, key: &str) -> Result<String>;
            fn lookup_n(&self, key: &str, n: usize) -> Vec<String>;
            fn lookup_bulk(&self, keys: &[String]) -> Vec<Option<String>>;
            fn rebalance(&mut self) -> Vec<Migration>;
            fn stats(&self) -> DistributionStats;
            fn node_count(&self) -> usize;
            fn clear(&mut self);
            fn serialize(&self) -> String;
            fn deserialize(&mut self, data: &str) -> bool;
        }
    }

    // Concrete hashers
    unsafe extern "C++" {
        // Ring hasher
        type RingHasher = crate::hasher::RingHasher;
        fn new_ring_hasher() -> UniquePtr<RingHasher>;
        fn add_node(self: Pin<&mut RingHasher>, node: Node);
        fn upsert_node(self: Pin<&mut RingHasher>, node: Node) -> bool;
        fn remove_node(self: Pin<&mut RingHasher>, id: &str);
        fn update_weight(self: Pin<&mut RingHasher>, id: &str, weight: u32) -> bool;
        fn has_node(self: &RingHasher, id: &str) -> bool;
        fn lookup(self: &RingHasher, key: &str) -> Option<String>;
        fn lookup_or_throw(self: &RingHasher, key: &str) -> Result<String>;
        fn lookup_n(self: &RingHasher, key: &str, n: usize) -> Vec<String>;
        fn lookup_bulk(self: &RingHasher, keys: &[String]) -> Vec<Option<String>>;
        fn rebalance(self: Pin<&mut RingHasher>) -> Vec<Migration>;
        fn stats(self: &RingHasher) -> DistributionStats;
        fn node_count(self: &RingHasher) -> usize;
        fn clear(self: Pin<&mut RingHasher>);
        fn serialize(self: &RingHasher) -> String;
        fn deserialize(self: Pin<&mut RingHasher>, data: &str) -> bool;

        // Jump hasher
        type JumpHasher = crate::hasher::JumpHasher;
        fn new_jump_hasher() -> UniquePtr<JumpHasher>;
        // ... similar methods

        // Maglev hasher
        type MaglevHasher = crate::hasher::MaglevHasher;
        fn new_maglev_hasher(table_size: usize) -> UniquePtr<MaglevHasher>;
        // ... similar methods

        // Rendezvous hasher
        type RendezvousHasher = crate::hasher::RendezvousHasher;
        fn new_rendezvous_hasher() -> UniquePtr<RendezvousHasher>;
        // ... similar methods

        // Multi-probe hasher
        type MultiProbeHasher = crate::hasher::MultiProbeHasher;
        fn new_multiprobe_hasher(probes: usize) -> UniquePtr<MultiProbeHasher>;
        // ... similar methods

        // Bounded loads hasher
        type BoundedLoadsHasher = crate::hasher::BoundedLoadsHasher;
        fn new_bounded_loads_hasher(vnodes: usize, capacity: usize) -> UniquePtr<BoundedLoadsHasher>;
        // ... similar methods

        // Ketama hasher
        type KetamaHasher = crate::hasher::KetamaHasher;
        fn new_ketama_hasher() -> UniquePtr<KetamaHasher>;
        // ... similar methods

        // Anchor hasher
        type AnchorHasher = crate::hasher::AnchorHasher;
        fn new_anchor_hasher() -> UniquePtr<AnchorHasher>;
        // ... similar methods

        // Lock-free hasher
        type LockFreeHasher = crate::hasher::LockFreeHasher;
        fn new_lockfree_hasher<H: Hasher>(hasher: UniquePtr<H>) -> UniquePtr<LockFreeHasher>;
        fn add_node(self: Pin<&mut LockFreeHasher>, node: Node);
        fn upsert_node(self: Pin<&mut LockFreeHasher>, node: Node) -> bool;
        fn remove_node(self: Pin<&mut LockFreeHasher>, id: &str);
        fn update_weight(self: Pin<&mut LockFreeHasher>, id: &str, weight: u32) -> bool;
        fn has_node(self: &LockFreeHasher, id: &str) -> bool;
        fn lookup(self: &LockFreeHasher, key: &str) -> Option<String>;
        fn lookup_or_throw(self: &LockFreeHasher, key: &str) -> Result<String>;
        fn lookup_n(self: &LockFreeHasher, key: &str, n: usize) -> Vec<String>;
        fn lookup_bulk(self: &LockFreeHasher, keys: &[String]) -> Vec<Option<String>>;
        fn rebalance(self: Pin<&mut LockFreeHasher>) -> Vec<Migration>;
        fn stats(self: &LockFreeHasher) -> DistributionStats;
        fn node_count(self: &LockFreeHasher) -> usize;
        fn clear(self: Pin<&mut LockFreeHasher>);
        fn serialize(self: &LockFreeHasher) -> String;
        fn deserialize(self: Pin<&mut LockFreeHasher>, data: &str) -> bool;
        fn version(self: &LockFreeHasher) -> u64;
        fn add_node_atomic(self: Pin<&mut LockFreeHasher>, node: Node);
        fn remove_node_atomic(self: Pin<&mut LockFreeHasher>, id: &str);
        fn update_weight_atomic(self: Pin<&mut LockFreeHasher>, id: &str, weight: u32) -> bool;
        fn create_snapshot(self: &LockFreeHasher) -> Snapshot;
        fn restore_snapshot(self: Pin<&mut LockFreeHasher>, snapshot: Snapshot) -> bool;

        struct Snapshot {
            version: u64,
            node_count: usize,
            timestamp: i64,
        }

        // Health checker
        type HealthChecker = crate::health::HealthChecker;
        fn new_health_checker(config: HealthCheckConfig) -> UniquePtr<HealthChecker>;
        fn register_node(self: Pin<&mut HealthChecker>, id: &str, check_fn: Box<dyn Fn(&str, &mut String, u64) -> bool + Send + Sync>, config: HealthCheckConfig);
        fn unregister_node(self: Pin<&mut HealthChecker>, id: &str);
        fn start(self: Pin<&mut HealthChecker>);
        fn stop(self: Pin<&mut HealthChecker>);
        fn get_health(self: &HealthChecker, id: &str) -> Option<NodeHealth>;
        fn get_healthy_nodes(self: &HealthChecker) -> Vec<String>;
        fn get_available_nodes(self: &HealthChecker) -> Vec<String>;
        fn set_draining(self: Pin<&mut HealthChecker>, id: &str, draining: bool);
        fn record_passive_error(self: Pin<&mut HealthChecker>, id: &str, error: &str);
        fn record_passive_success(self: Pin<&mut HealthChecker>, id: &str);
        fn passive_error_count(self: &HealthChecker, id: &str) -> usize;
        fn set_callback(self: Pin<&mut HealthChecker>, callback: Box<dyn Fn(&str, HealthStatus, HealthStatus) + Send + Sync>);

        // Health-aware hasher
        type HealthAwareHasher = crate::hasher::HealthAwareHasher;
        fn new_health_aware_hasher<H: Hasher>(hasher: UniquePtr<H>, health: &HealthChecker) -> UniquePtr<HealthAwareHasher>;
        // ... similar methods

        // SIMD hasher
        type AdaptiveHasher = crate::simd::AdaptiveHasher;
        fn new_adaptive_hasher() -> UniquePtr<AdaptiveHasher>;
        fn new_adaptive_hasher_with_type(preferred: SIMDType) -> UniquePtr<AdaptiveHasher>;
        fn hash(self: &AdaptiveHasher, data: &str) -> u64;
        fn hash_with_seed(self: &AdaptiveHasher, data: &str, seed: u64) -> u64;
        fn hash_batch(self: &AdaptiveHasher, inputs: &[String]) -> Vec<u64>;
        fn type(self: &AdaptiveHasher) -> SIMDType;
        fn type_name(self: &AdaptiveHasher) -> String;
    }
}

// node.rs
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct Node {
    pub id: String,
    pub address: String,
    pub weight: u32,
    pub zone: String,
    pub metadata: std::collections::HashMap<String, String>,
}

impl Node {
    pub fn new(id: impl Into<String>, address: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            address: address.into(),
            weight: 100,
            zone: String::new(),
            metadata: std::collections::HashMap::new(),
        }
    }

    pub fn with_weight(mut self, weight: u32) -> Self {
        self.weight = weight;
        self
    }

    pub fn with_zone(mut self, zone: impl Into<String>) -> Self {
        self.zone = zone.into();
        self
    }

    pub fn with_metadata(mut self, metadata: std::collections::HashMap<String, String>) -> Self {
        self.metadata = metadata;
        self
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct Migration {
    pub from: String,
    pub to: String,
    pub key: String,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct DistributionStats {
    pub total_keys: usize,
    pub unique_nodes_used: usize,
    pub mean_keys_per_node: f64,
    pub stddev_keys_per_node: f64,
    pub skew: f64,
    pub max_load: f64,
    pub min_load: f64,
    pub migration_count: u64,
    pub avg_lookup_ns: f64,
    pub p50_lookup_ns: f64,
    pub p90_lookup_ns: f64,
    pub p99_lookup_ns: f64,
    pub p999_lookup_ns: f64,
}

// error.rs
#[derive(Debug, thiserror::Error)]
pub enum ConsistentHashError {
    #[error("Node not found: {0}")]
    NodeNotFound(String),

    #[error("Invalid weight: {0} (must be 1..1000)")]
    InvalidWeight(String),

    #[error("Empty ring: no nodes available")]
    EmptyRing,

    #[error("No active nodes available")]
    NoActiveNodes,

    #[error("Invalid argument: {0}")]
    InvalidArgument(String),

    #[error("Serialization error: {0}")]
    SerializationError(String),

    #[error("Health check error: {0}")]
    HealthCheckError(String),

    #[error("Lock-free operation failed: {0}")]
    LockFreeError(String),

    #[error("SIMD error: {0}")]
    SIMDError(String),
}

pub type Result<T> = std::result::Result<T, ConsistentHashError>;

// health.rs
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum HealthStatus {
    Unknown,
    Healthy,
    Degraded,
    Unhealthy,
    Draining,
}

impl Default for HealthStatus {
    fn default() -> Self {
        HealthStatus::Unknown
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct HealthCheckConfig {
    pub interval_ms: u64,
    pub timeout_ms: u64,
    pub failure_threshold: usize,
    pub success_threshold: usize,
    pub initial_delay_ms: u64,
    pub passive_checks: bool,
    pub passive_error_threshold: usize,
    pub passive_window_secs: u64,
}

impl Default for HealthCheckConfig {
    fn default() -> Self {
        Self {
            interval_ms: 5000,
            timeout_ms: 2000,
            failure_threshold: 3,
            success_threshold: 2,
            initial_delay_ms: 1000,
            passive_checks: true,
            passive_error_threshold: 10,
            passive_window_secs: 60,
        }
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct NodeHealth {
    pub node_id: String,
    pub status: HealthStatus,
    pub last_check: i64,
    pub last_status_change: i64,
    pub consecutive_failures: usize,
    pub consecutive_successes: usize,
    pub total_checks: u64,
    pub total_failures: u64,
    pub total_passive_errors: u64,
    pub last_passive_error: i64,
    pub last_error_message: String,
    pub latency_ms: f64,
}

impl NodeHealth {
    pub fn is_healthy(&self) -> bool {
        self.status == HealthStatus::Healthy
    }

    pub fn is_available(&self) -> bool {
        self.status == HealthStatus::Healthy || self.status == HealthStatus::Degraded
    }
}

// hasher.rs
use crate::{ConsistentHashError, Result, node::{Node, Migration, DistributionStats}};

pub trait Hasher {
    fn add_node(&mut self, node: Node);
    fn upsert_node(&mut self, node: Node) -> bool;
    fn remove_node(&mut self, id: &str);
    fn update_weight(&mut self, id: &str, weight: u32) -> bool;
    fn has_node(&self, id: &str) -> bool;
    fn lookup(&self, key: &str) -> Option<String>;
    fn lookup_or_throw(&self, key: &str) -> Result<String>;
    fn lookup_n(&self, key: &str, n: usize) -> Vec<String>;
    fn lookup_bulk(&self, keys: &[String]) -> Vec<Option<String>>;
    fn rebalance(&mut self) -> Vec<Migration>;
    fn stats(&self) -> DistributionStats;
    fn node_count(&self) -> usize;
    fn clear(&mut self);
    fn serialize(&self) -> String;
    fn deserialize(&mut self, data: &str) -> bool;
}

// Concrete hasher implementations
pub struct RingHasher { /* ... */ }
pub struct JumpHasher { /* ... */ }
pub struct MaglevHasher { /* ... */ }
pub struct RendezvousHasher { /* ... */ }
pub struct MultiProbeHasher { /* ... */ }
pub struct BoundedLoadsHasher { /* ... */ }
pub struct KetamaHasher { /* ... */ }
pub struct AnchorHasher { /* ... */ }

// Lock-free hasher
pub struct LockFreeHasher { /* ... */ }

impl LockFreeHasher {
    pub fn new<H: Hasher>(hasher: H) -> Self { /* ... */ }
    pub fn version(&self) -> u64 { /* ... */ }
    pub fn add_node_atomic(&mut self, node: Node) { /* ... */ }
    pub fn remove_node_atomic(&mut self, id: &str) { /* ... */ }
    pub fn update_weight_atomic(&mut self, id: &str, weight: u32) -> bool { /* ... */ }
    pub fn create_snapshot(&self) -> Snapshot { /* ... */ }
    pub fn restore_snapshot(&mut self, snapshot: Snapshot) -> bool { /* ... */ }
}

pub struct Snapshot {
    pub version: u64,
    pub node_count: usize,
    pub timestamp: i64,
}

// Health-aware hasher
pub struct HealthAwareHasher { /* ... */ }

// SIMD hasher
pub struct AdaptiveHasher { /* ... */ }

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SIMDType {
    HighwayHash,
    XxHash3,
    Fnv1a,
}

impl AdaptiveHasher {
    pub fn new() -> Self { /* ... */ }
    pub fn with_preferred_type(preferred: SIMDType) -> Self { /* ... */ }
    pub fn hash(&self, data: &str) -> u64 { /* ... */ }
    pub fn hash_with_seed(&self, data: &str, seed: u64) -> u64 { /* ... */ }
    pub fn hash_batch(&self, inputs: &[String]) -> Vec<u64> { /* ... */ }
    pub fn hash_type(&self) -> SIMDType { /* ... */ }
    pub fn type_name(&self) -> &str { /* ... */ }
}

// Health checker
pub struct HealthChecker { /* ... */ }

impl HealthChecker {
    pub fn new(config: HealthCheckConfig) -> Self { /* ... */ }
    pub fn register_node<F>(&mut self, id: &str, check_fn: F, config: HealthCheckConfig) 
    where F: Fn(&str, &mut String, u64) -> bool + Send + Sync + 'static { /* ... */ }
    pub fn unregister_node(&mut self, id: &str) { /* ... */ }
    pub fn start(&mut self) { /* ... */ }
    pub fn stop(&mut self) { /* ... */ }
    pub fn get_health(&self, id: &str) -> Option<NodeHealth> { /* ... */ }
    pub fn get_healthy_nodes(&self) -> Vec<String> { /* ... */ }
    pub fn get_available_nodes(&self) -> Vec<String> { /* ... */ }
    pub fn set_draining(&mut self, id: &str, draining: bool) { /* ... */ }
    pub fn record_passive_error(&mut self, id: &str, error: &str) { /* ... */ }
    pub fn record_passive_success(&mut self, id: &str) { /* ... */ }
    pub fn passive_error_count(&self, id: &str) -> usize { /* ... */ }
    pub fn set_callback<F>(&mut self, callback: F) 
    where F: Fn(&str, HealthStatus, HealthStatus) + Send + Sync + 'static { /* ... */ }
}

// Health-aware hasher
pub struct HealthAwareHasher { /* ... */ }

// Factory functions
pub fn make_lockfree<H: Hasher>(hasher: H) -> LockFreeHasher { /* ... */ }
pub fn make_health_aware<H: Hasher>(hasher: H, health: &HealthChecker) -> HealthAwareHasher { /* ... */ }
pub fn make_lockfree_health_aware<H: Hasher>(hasher: H, health: &HealthChecker) -> LockFreeHasher { /* ... */ }

// SIMD hasher
pub mod simd {
    use super::*;
    
    pub struct AdaptiveHasher { /* ... */ }
    
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum SIMDType {
        HighwayHash,
        XxHash3,
        Fnv1a,
    }
    
    impl AdaptiveHasher {
        pub fn new() -> Self { /* ... */ }
        pub fn with_preferred_type(preferred: SIMDType) -> Self { /* ... */ }
        pub fn hash(&self, data: &str) -> u64 { /* ... */ }
        pub fn hash_with_seed(&self, data: &str, seed: u64) -> u64 { /* ... */ }
        pub fn hash_batch(&self, inputs: &[String]) -> Vec<u64> { /* ... */ }
        pub fn hash_type(&self) -> SIMDType { /* ... */ }
        pub fn type_name(&self) -> &str { /* ... */ }
    }
}

// tests.rs
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_ring_hasher() {
        let mut hasher = RingHasher::new();
        hasher.add_node(Node::new("node1", "10.0.0.1:11211"));
        hasher.add_node(Node::new("node2", "10.0.0.2:11211"));
        
        let result = hasher.lookup("test-key");
        assert!(result.is_some());
    }
    
    #[test]
    fn test_jump_hasher() {
        let mut hasher = JumpHasher::new();
        hasher.add_node(Node::new("node1", "10.0.0.1:11211"));
        hasher.add_node(Node::new("node2", "10.0.0.2:11211"));
        
        let result = hasher.lookup("test-key");
        assert!(result.is_some());
    }
    
    #[test]
    fn test_ketama_hasher() {
        let mut hasher = KetamaHasher::new();
        hasher.add_node(Node::new("node1", "10.0.0.1:11211").with_weight(100));
        hasher.add_node(Node::new("node2", "10.0.0.2:11211").with_weight(100));
        
        let result = hasher.lookup("test-key");
        assert!(result.is_some());
    }
    
    #[test]
    fn test_anchor_hasher() {
        let mut hasher = AnchorHasher::new();
        hasher.add_node(Node::new("node1", "10.0.0.1:11211"));
        hasher.add_node(Node::new("node2", "10.0.0.2:11211"));
        
        let result = hasher.lookup("test-key");
        assert!(result.is_some());
    }
    
    #[test]
    fn test_lockfree_hasher() {
        let base = RingHasher::new();
        let mut lf = LockFreeHasher::new(base);
        
        lf.add_node_atomic(Node::new("node1", "10.0.0.1:11211"));
        let result = lf.lookup("test-key");
        assert!(result.is_some());
    }
    
    #[test]
    fn test_adaptive_hasher() {
        let hasher = crate::simd::AdaptiveHasher::new();
        let hash1 = hasher.hash("test-key");
        let hash2 = hasher.hash("test-key");
        assert_eq!(hash1, hash2);
        
        let hashes = hasher.hash_batch(vec!["key1".to_string(), "key2".to_string()]);
        assert_eq!(hashes.len(), 2);
    }
    
    #[test]
    fn test_weighted_distribution() {
        let mut hasher = KetamaHasher::new();
        hasher.add_node(Node::new("heavy", "10.0.0.1:11211").with_weight(300));
        hasher.add_node(Node::new("light", "10.0.0.2:11211").with_weight(100));
        
        let mut counts = std::collections::HashMap::new();
        for i in 0..10000 {
            if let Some(node) = hasher.lookup(&format!("key-{}", i)) {
                *counts.entry(node).or_insert(0) += 1;
            }
        }
        
        let ratio = counts["heavy"] as f64 / counts["light"] as f64;
        assert!(ratio > 2.0 && ratio < 4.0);
    }
}