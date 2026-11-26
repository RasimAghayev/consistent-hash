#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
  using key_type = Key;
  using result_type = uint64_t;

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

// Composable algebraic hasher: XOR composition of two hashers.
// Provides better avalanche effect while preserving uniform distribution.
template <typename Hasher1, typename Hasher2>
requires std::same_as<typename Hasher1::key_type, typename Hasher2::key_type>
struct ComposedHasher {
  using key_type = typename Hasher1::key_type;
  using result_type = uint64_t;

  Hasher1 h1;
  Hasher2 h2;

  constexpr ComposedHasher() noexcept requires std::is_default_constructible_v<
      Hasher1> && std::is_default_constructible_v<Hasher2>
  = default;

  constexpr ComposedHasher(Hasher1 h1_, Hasher2 h2_) noexcept
      : h1(std::move(h1_)), h2(std::move(h2_)) {}

  uint64_t operator()(const key_type &key) const noexcept {
    return h1(key) ^ h2(key);
  }

  // Compose with another hasher (left-associative).
  template <typename Hasher3>
  constexpr auto compose(Hasher3 h3) const noexcept {
    return ComposedHasher<ComposedHasher<Hasher1, Hasher2>, Hasher3>{
        *this, std::move(h3)};
  }
};

// XOR operator for ergonomic composition.
template <typename Hasher1, typename Hasher2>
requires std::same_as<typename Hasher1::key_type, typename Hasher2::key_type>
constexpr auto operator^(Hasher1 h1, Hasher2 h2) noexcept {
  return ComposedHasher<Hasher1, Hasher2>{std::move(h1), std::move(h2)};
}

// Combine two 64-bit hashes (Boost-style).
inline uint64_t hash_combine(uint64_t a, uint64_t b) noexcept {
  a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
  return a;
}

// Perfect hash for compile-time known keys (mph-style interface).
// Requires C++20 constexpr and fixed key set.
template <const auto &Entries> struct PerfectHasher {
  static_assert(Entries.size() > 0, "Entries must not be empty");

  using key_type = std::remove_cvref_t<decltype(Entries[0].first)>;
  using mapped_type = std::remove_cvref_t<decltype(Entries[0].second)>;
  using result_type = uint64_t;

  // Perfect lookup - assumes valid key, returns mapped value directly.
  [[nodiscard]] static constexpr mapped_type
  lookup(const key_type &key) noexcept {
    for (const auto &[k, v] : Entries) {
      if (k == key)
        return v;
    }
    return mapped_type{}; // Should not reach here for valid keys.
  }

  // Safe lookup returning optional.
  [[nodiscard]] static constexpr std::optional<mapped_type>
  find(const key_type &key) noexcept {
    for (const auto &[k, v] : Entries) {
      if (k == key)
        return v;
    }
    return std::nullopt;
  }
};

} // namespace consistent_hash