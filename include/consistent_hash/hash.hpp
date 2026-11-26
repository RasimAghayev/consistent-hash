#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

namespace consistent_hash {
namespace detail {

// FNV-1a 64-bit hash over raw bytes.
inline uint64_t fnv1a_64(const unsigned char *data, size_t len) noexcept {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (size_t i = 0; i < len; ++i) {
    h ^= static_cast<uint64_t>(data[i]);
    h *= 0x100000001b3ULL;
  }
  return h;
}

inline uint64_t fnv1a_64(std::string_view sv) noexcept {
  return fnv1a_64(reinterpret_cast<const unsigned char *>(sv.data()),
                  sv.size());
}

// Avalanche finalizer (splitmix64). Fixes FNV-1a clustering on similar keys
// (e.g. "key:0".."key:9999" hash to a narrow range) so consistent-hash rings
// distribute them across the full 64-bit space.
inline uint64_t mix64(uint64_t x) noexcept {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

} // namespace detail

// Default key hasher. Strings are hashed by content; arithmetic types by
// bytes; everything else via std::hash mixed into 64 bits.
template <typename Key> struct DefaultHasher {
  uint64_t operator()(const Key &key) const noexcept {
    uint64_t h;
    if constexpr (std::is_same_v<Key, std::string> ||
                  std::is_same_v<Key, std::string_view>) {
      h = detail::fnv1a_64(reinterpret_cast<const unsigned char *>(key.data()),
                           key.size());
    } else if constexpr (std::is_arithmetic_v<Key>) {
      h = detail::fnv1a_64(reinterpret_cast<const unsigned char *>(&key),
                           sizeof(Key));
    } else {
      size_t hh = std::hash<Key>{}(key);
      h = detail::fnv1a_64(reinterpret_cast<const unsigned char *>(&hh),
                           sizeof(hh));
    }
    return detail::mix64(h);
  }
};

// Combine two 64-bit hashes.
inline uint64_t hash_combine(uint64_t a, uint64_t b) noexcept {
  a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
  return a;
}

} // namespace consistent_hash
