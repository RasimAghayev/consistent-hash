#pragma once

#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "consistent_hash/types.hpp"

namespace consistent_hash {

// Thread-safe registry of nodes. Multiple readers, exclusive writer.
// Used to orchestrate node lifecycle across one or more hashers.
class NodeManager {
public:
    using NodeId = std::string;

    void add_node(const Node<std::string>& node);
    void remove_node(const NodeId& id);
    bool has_node(const NodeId& id) const;
    Node<std::string> get_node(const NodeId& id) const;
    std::vector<Node<std::string>> list_nodes() const;
    size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    std::map<NodeId, Node<std::string>> nodes_;
};

}  // namespace consistent_hash
