#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace consistent_hash::simd {

// CPU feature detection
struct CpuFeatures {
  bool has_sse2 = false;
  bool has_sse41 = false;
  bool has_sse42 = false;
  bool has_avx = false;
  bool has_avx2 = false;
  bool has_avx512f = false;
  bool has_aes = false;
  bool has_sha = false;
  bool has_neon = false;
  bool has_sve = false;

  static const CpuFeatures& detect() noexcept;
};

// Forward declaration for xxHash3
extern "C" uint64_t XXH3_64bits_withSeed(const void* input, size_t len, uint64_t seed);

// HighwayHash implementation (portable, fast, strong)
class HighwayHash {
public:
  using key_type = std::array<uint64_t, 4>;
  using result_type = uint64_t;

  explicit HighwayHash(key_type key = {0x0123456789abcdef, 0xfedcba9876543210,
                                        0x13579bdf2468ace0, 0x02468ace13579bdf}) noexcept
      : key_(key) {}

  // Hash a single string
  uint64_t operator()(std::string_view data) const noexcept {
    return hash(data.data(), data.size());
  }

  // Hash with seed
  uint64_t operator()(std::string_view data, uint64_t seed) const noexcept {
    key_type seeded_key = key_;
    seeded_key[0] ^= seed;
    return HighwayHash{seeded_key}(data);
  }

  // Batch hash multiple keys
  void hash_batch(const std::string_view* inputs, uint64_t* outputs, size_t count) const noexcept;

private:
  key_type key_;

  uint64_t hash(const void* data, size_t len) const noexcept;

  // HighwayHash core (simplified - real impl uses HighwayHash library)
  static uint64_t hh_hash(const key_type& key, const void* data, size_t len) noexcept;
};

// xxHash3 implementation (extremely fast, good quality)
class XxHash3 {
public:
  using result_type = uint64_t;

  explicit XxHash3(uint64_t seed = 0) noexcept : seed_(seed) {}

  uint64_t operator()(std::string_view data) const noexcept {
    return XXH3_64bits_withSeed(data.data(), data.size(), seed_);
  }

  uint64_t operator()(std::string_view data, uint64_t seed) const noexcept {
    return XXH3_64bits_withSeed(data.data(), data.size(), seed);
  }

  void hash_batch(const std::string_view* inputs, uint64_t* outputs, size_t count) const noexcept;

private:
  uint64_t seed_;
};

// FNV-1a (fallback, no SIMD needed)
class Fnv1a {
public:
  using result_type = uint64_t;

  constexpr uint64_t operator()(std::string_view data) const noexcept {
    uint64_t hash = 0xcbf29ce484222325;
    for (char c : data) {
      const unsigned char uc = static_cast<unsigned char>(c);
      hash ^= uc;
      hash *= 0x100000001b3;
    }
    return hash;
  }
};

// Adaptive hasher - picks best available implementation at runtime
class AdaptiveHasher {
public:
  using result_type = uint64_t;

  enum class Type { HighwayHash, XxHash3, Fnv1a };

  AdaptiveHasher() noexcept {
#if defined(CONSISTENT_HASH_HAS_AVX2) && CONSISTENT_HASH_HAS_AVX2
    type_ = Type::HighwayHash;
#elif defined(CONSISTENT_HASH_HAS_SSE42) && CONSISTENT_HASH_HAS_SSE42
    type_ = Type::XxHash3;
#elif defined(CONSISTENT_HASH_HAS_NEON) && CONSISTENT_HASH_HAS_NEON
    type_ = Type::XxHash3;  // xxHash3 has NEON optimizations
#else
    type_ = Type::Fnv1a;
#endif
  }

  explicit AdaptiveHasher(Type preferred_type) noexcept : type_(preferred_type) {
    const auto& cpu = CpuFeatures::detect();
    if (type_ == Type::HighwayHash && !cpu.has_avx2) type_ = Type::XxHash3;
    if (type_ == Type::XxHash3 && !cpu.has_sse42) type_ = Type::Fnv1a;
  }

  uint64_t operator()(std::string_view data) const noexcept {
    switch (type_) {
      case Type::HighwayHash: return highway_(data);
      case Type::XxHash3: return xxh3_(data);
      case Type::Fnv1a: return fnv1a_(data);
    }
    return fnv1a_(data);
  }

  uint64_t operator()(std::string_view data, uint64_t seed) const noexcept {
    switch (type_) {
      case Type::HighwayHash: return highway_(data, seed);
      case Type::XxHash3: return xxh3_(data, seed);
      case Type::Fnv1a: return fnv1a_(data);
    }
    return fnv1a_(data);
  }

  void hash_batch(const std::string_view* inputs, uint64_t* outputs, size_t count) const noexcept {
    switch (type_) {
      case Type::HighwayHash: highway_.hash_batch(inputs, outputs, count); break;
      case Type::XxHash3: xxh3_.hash_batch(inputs, outputs, count); break;
      case Type::Fnv1a:
        for (size_t i = 0; i < count; ++i) outputs[i] = fnv1a_(inputs[i]);
        break;
    }
  }

  Type type() const noexcept { return type_; }
  const char* type_name() const noexcept {
    switch (type_) {
      case Type::HighwayHash: return "HighwayHash (AVX2)";
      case Type::XxHash3: return "xxHash3 (SSE4.2)";
      case Type::Fnv1a: return "FNV-1a (scalar)";
    }
    return "unknown";
  }

private:
  Type type_;
  HighwayHash highway_;
  XxHash3 xxh3_;
  Fnv1a fnv1a_;
};

// Hash combiners for SIMD-friendly operations
namespace combine {
inline uint64_t xor_combine(uint64_t a, uint64_t b) noexcept {
  return a ^ b;
}

inline uint64_t splitmix64_combine(uint64_t a, uint64_t b) noexcept {
  uint64_t z = a + 0x9e3779b97f4a7c15;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31) ^ b;
}

inline uint64_t hash_combine(uint64_t a, uint64_t b) noexcept {
  return splitmix64_combine(a, b);
}
} // namespace combine

} // namespace consistent_hash::simd