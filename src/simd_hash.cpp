#include "consistent_hash/simd_hash.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#if defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

namespace consistent_hash::simd {

// CPU feature detection implementation
const CpuFeatures& CpuFeatures::detect() noexcept {
  static CpuFeatures features = []() {
    CpuFeatures f;
#if defined(__x86_64__) || defined(_M_X64)
    int cpu_info[4] = {0};
    
    // CPUID leaf 1
#if defined(_MSC_VER)
    __cpuid(cpu_info, 1);
#else
    __cpuid(1, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
    f.has_sse2 = (cpu_info[3] & (1 << 26)) != 0;
    f.has_sse41 = (cpu_info[2] & (1 << 19)) != 0;
    f.has_sse42 = (cpu_info[2] & (1 << 20)) != 0;
    
    // CPUID leaf 7
#if defined(_MSC_VER)
    __cpuid(cpu_info, 7);
#else
    __cpuid(7, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
    f.has_avx2 = (cpu_info[1] & (1 << 5)) != 0;
    f.has_avx512f = (cpu_info[1] & (1 << 16)) != 0;
    f.has_sha = (cpu_info[1] & (1 << 29)) != 0;
    
    // CPUID leaf 1 for AES
    f.has_aes = (cpu_info[2] & (1 << 25)) != 0;
    
    // Check AVX (OS support)
#if defined(_MSC_VER)
    __cpuid(cpu_info, 1);
#else
    __cpuid(1, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
    bool os_avx = (cpu_info[2] & (1 << 27)) != 0; // OSXSAVE
    bool avx = (cpu_info[2] & (1 << 28)) != 0;    // AVX
    f.has_avx = os_avx && avx;
    
#elif defined(__aarch64__)
    // ARM64 feature detection via getauxval or /proc/cpuinfo
    // Simplified - in production use getauxval(AT_HWCAP)
    f.has_neon = true; // NEON is baseline on ARMv8
    // SVE detection would require getauxval(AT_HWCAP2)
#endif
    return f;
  }();
  return features;
}

// HighwayHash implementation (simplified portable version)
// Real implementation would use google/highwayhash library
uint64_t HighwayHash::hh_hash(const key_type& key, const void* data, size_t len) noexcept {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  
  // HighwayHash state (4x64-bit)
  uint64_t v0 = key[0] ^ 0xdbe6d5d5fe4cce2f;
  uint64_t v1 = key[1] ^ 0xa4093822299f31d0;
  uint64_t v2 = key[2] ^ 0x13198a2e03707344;
  uint64_t v3 = key[3] ^ 0x243f6a8885a308d3;
  
  // Process 32 bytes at a time (simplified)
  const size_t blocks = len / 32;
  for (size_t i = 0; i < blocks; ++i) {
    // Load 32 bytes
    uint64_t m0, m1, m2, m3;
    std::memcpy(&m0, bytes + i * 32 + 0, 8);
    std::memcpy(&m1, bytes + i * 32 + 8, 8);
    std::memcpy(&m2, bytes + i * 32 + 16, 8);
    std::memcpy(&m3, bytes + i * 32 + 24, 8);
    
    // HighwayHash permutation (simplified)
    v0 += m0; v1 += m1; v2 += m2; v3 += m3;
    v0 = (v0 << 32) | (v0 >> 32);
    v1 = (v1 << 32) | (v1 >> 32);
    v2 = (v2 << 32) | (v2 >> 32);
    v3 = (v3 << 32) | (v3 >> 32);
    v0 ^= v1; v2 ^= v3;
    v0 += v2; v1 += v3;
    v0 = (v0 << 24) | (v0 >> 40);
    v1 = (v1 << 24) | (v1 >> 40);
    v2 = (v2 << 24) | (v2 >> 40);
    v3 = (v3 << 24) | (v3 >> 40);
  }
  
  // Process remaining bytes
  size_t rem = len % 32;
  if (rem > 0) {
    uint64_t m0 = 0, m1 = 0, m2 = 0, m3 = 0;
    std::memcpy(&m0, bytes + blocks * 32, std::min(rem, size_t(8)));
    if (rem > 8) std::memcpy(&m1, bytes + blocks * 32 + 8, std::min(rem - 8, size_t(8)));
    if (rem > 16) std::memcpy(&m2, bytes + blocks * 32 + 16, std::min(rem - 16, size_t(8)));
    if (rem > 24) std::memcpy(&m3, bytes + blocks * 32 + 24, rem - 24);
    
    v0 += m0; v1 += m1; v2 += m2; v3 += m3;
  }
  
  // Finalization
  v0 ^= v1; v2 ^= v3;
  v0 += v2; v1 += v3;
  v0 = (v0 << 32) | (v0 >> 32);
  v1 = (v1 << 32) | (v1 >> 32);
  v0 ^= v1;
  
  return v0 ^ (len * 0x9e3779b97f4a7c15);
}

uint64_t HighwayHash::hash(const void* data, size_t len) const noexcept {
  return hh_hash(key_, data, len);
}

void HighwayHash::hash_batch(const std::string_view* inputs, uint64_t* outputs, size_t count) const noexcept {
  for (size_t i = 0; i < count; ++i) {
    outputs[i] = hash(inputs[i].data(), inputs[i].size());
  }
}

// xxHash3 implementation (using built-in or simplified)
extern "C" uint64_t XXH3_64bits_withSeed(const void* input, size_t len, uint64_t seed);

void XxHash3::hash_batch(const std::string_view* inputs, uint64_t* outputs, size_t count) const noexcept {
  for (size_t i = 0; i < count; ++i) {
    outputs[i] = (*this)(inputs[i]);
  }
}

// If xxHash not available, provide fallback
#ifndef XXH3_64bits_withSeed
extern "C" uint64_t XXH3_64bits_withSeed(const void* input, size_t len, uint64_t seed) {
  // Fallback to FNV-1a with seed
  const uint8_t* data = static_cast<const uint8_t*>(input);
  uint64_t hash = seed ^ 0xcbf29ce484222325;
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 0x100000001b3;
  }
  return hash;
}
#endif

} // namespace consistent_hash::simd