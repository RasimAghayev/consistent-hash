#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "consistent_hash/bounded_loads.hpp"
#include "consistent_hash/jump.hpp"
#include "consistent_hash/maglev.hpp"
#include "consistent_hash/multiprobe.hpp"
#include "consistent_hash/rendezvous.hpp"
#include "consistent_hash/ring.hpp"

namespace consistent_hash {

enum class Algorithm {
  Ring,
  Jump,
  Maglev,
  Rendezvous,
  MultiProbe,
  BoundedLoads
};

// Type-erased factory for creating hashers by algorithm.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class ConsistentHashFactory {
public:
  using Base = ConsistentHasher<NodeId, Key, Hasher>;
  using NodeType = typename Base::NodeType;

  static std::unique_ptr<Base> create(Algorithm algo, size_t param1 = 0,
                                      size_t param2 = 0) {
    switch (algo) {
    case Algorithm::Ring:
      return std::make_unique<RingHasher<NodeId, Key, Hasher>>(
          param1 == 0 ? 150 : param1);
    case Algorithm::Jump:
      return std::make_unique<JumpHasher<NodeId, Key, Hasher>>();
    case Algorithm::Maglev:
      return std::make_unique<MaglevHasher<NodeId, Key, Hasher>>(
          param1 == 0 ? MaglevHasher<NodeId, Key, Hasher>::kDefaultTableSize
                      : param1);
    case Algorithm::Rendezvous:
      return std::make_unique<RendezvousHasher<NodeId, Key, Hasher>>();
    case Algorithm::MultiProbe:
      return std::make_unique<MultiProbeHasher<NodeId, Key, Hasher>>(
          param1 == 0 ? MultiProbeHasher<NodeId, Key, Hasher>::kDefaultProbes
                      : param1);
    case Algorithm::BoundedLoads:
      return std::make_unique<BoundedLoadsHasher<NodeId, Key, Hasher>>(
          param1 == 0 ? 150 : param1, param2 == 0 ? 1000 : param2);
    }
    return nullptr;
  }

  static std::string algorithm_name(Algorithm algo) {
    switch (algo) {
    case Algorithm::Ring:
      return "ring";
    case Algorithm::Jump:
      return "jump";
    case Algorithm::Maglev:
      return "maglev";
    case Algorithm::Rendezvous:
      return "rendezvous";
    case Algorithm::MultiProbe:
      return "multiprobe";
    case Algorithm::BoundedLoads:
      return "bounded_loads";
    }
    return "unknown";
  }
};

// Convenience type aliases for common configurations.
template <typename NodeId = std::string, typename Key = std::string>
using Ring = RingHasher<NodeId, Key>;

template <typename NodeId = std::string, typename Key = std::string>
using Jump = JumpHasher<NodeId, Key>;

template <typename NodeId = std::string, typename Key = std::string>
using Maglev = MaglevHasher<NodeId, Key>;

template <typename NodeId = std::string, typename Key = std::string>
using Rendezvous = RendezvousHasher<NodeId, Key>;

template <typename NodeId = std::string, typename Key = std::string>
using MultiProbe = MultiProbeHasher<NodeId, Key>;

template <typename NodeId = std::string, typename Key = std::string>
using BoundedLoads = BoundedLoadsHasher<NodeId, Key>;

} // namespace consistent_hash