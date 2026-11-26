#pragma once

#include <concepts>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

#include "consistent_hash/types.hpp"

namespace consistent_hash {

template <typename T>
concept Hashable = requires(T a) {
  { std::hash<T>{}(a) } -> std::convertible_to<size_t>;
}
|| std::is_arithmetic_v<T> || std::is_same_v<T, std::string> ||
    std::is_same_v<T, std::string_view>;

template <typename T>
concept NodeIdType =
    std::copyable<T> && std::equality_comparable<T> && Hashable<T>;

template <typename T>
concept KeyType = std::copyable<T> && Hashable<T>;

template <typename Hasher, typename Key>
concept KeyHasher = requires(const Hasher &h, const Key &k) {
  { h(k) } -> std::convertible_to<uint64_t>;
};

template <typename T>
concept Serializable = requires(const T &t, std::format_context &ctx) {
  { std::format(ctx, "{}", t) } -> std::same_as<std::format_context::iterator>;
};

template <typename T>
concept RangeCompatible = std::ranges::input_range<T> && requires(const T &r) {
  {std::ranges::begin(r)};
  {std::ranges::end(r)};
};

template <typename T>
concept NodeType = requires(T n) {
  { n.id } -> std::convertible_to<typename T::id_type>;
  { n.address } -> std::convertible_to<std::string>;
  { n.weight } -> std::convertible_to<uint32_t>;
  { n.zone } -> std::convertible_to<std::string>;
  { n.status } -> std::convertible_to<NodeStatus>;
  { n.metadata } -> std::convertible_to<std::map<std::string, std::string>>;
  requires Serializable<typename T::id_type>;
};

template <typename T>
concept KeyHasherComposable = KeyHasher<T, typename T::key_type> &&
    requires(const T &a, const T &b) {
  { a ^ b } -> std::convertible_to<typename T::result_type>;
  { a.compose(b) } -> std::same_as<T>;
};

} // namespace consistent_hash