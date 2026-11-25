#include "consistent_hash/node_manager.hpp"

#include <stdexcept>

namespace consistent_hash {

void NodeManager::add_node(const Node<std::string>& node) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    nodes_[node.id] = node;
}

void NodeManager::remove_node(const NodeId& id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    nodes_.erase(id);
}

bool NodeManager::has_node(const NodeId& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.find(id) != nodes_.end();
}

Node<std::string> NodeManager::get_node(const NodeId& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
        throw std::runtime_error("NodeManager: node not found: " + id);
    }
    return it->second;
}

std::vector<Node<std::string>> NodeManager::list_nodes() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<Node<std::string>> result;
    result.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_) {
        result.push_back(node);
    }
    return result;
}

size_t NodeManager::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.size();
}

}  // namespace consistent_hash
