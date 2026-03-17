/**
 * Copyright 2025 Bruce Ide
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fr/autocrud/RelationTypes.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <meta>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace fr::autocrud {

  /**
   * A generic node type. This is basically the same one from
   * my RequirementsManager project. All autocrud data
   * should derive from this.
   */
  
  struct Node : public std::enable_shared_from_this<Node> {
  public:
    using Type = Node;
    using PtrType = std::shared_ptr<Type>;
    // Type to store in up/down vectors. This will be a std::pair of
    // PtrType and Relationship, which is an enum defined in
    // RelationshipTypes.h. This will allow you to tag up/down relationships
    // with hopefully useful metadata about the relationship
    using NodeRelation = std::pair<PtrType, Relationship>;
    
  protected:

    // Private function to traverse graph. Public entrypoint does not contain visisted
    // reference
    
    virtual void traverse(std::function<void(PtrType)> eachNodeFn, std::unordered_map<std::string, PtrType> &visited) {
      // Handle this node

      // Force init if not already initted
      if (!this->initted) {
        this->init();
      }

      if (! visited.contains(this->idString())) {
        visited[this->idString()] = this->shared_from_this();
        if (eachNodeFn) {
          eachNodeFn(this->shared_from_this());
        }
      }

      // Handle up nodes
      for (auto upNode : up) {
        if (! visited.contains(upNode.first->idString())) {
          upNode.first->traverse(eachNodeFn, visited);
        }
      }

      // Handle down nodes
      for (auto downNode : down) {
        if (! visited.contains(downNode.first->idString())) {
          downNode.first->traverse(eachNodeFn, visited);
        }
      }
    }

  public:

    // Lock nodeMutex before changing values, serializing/deserializing
    // or storing the node in a database. Always release the
    // mutex as soon as possible after locking it.
    mutable std::mutex nodeMutex;
    // Use up for things like parent(s), required-by, owner(s), etc
    std::vector<NodeRelation> up;
    // Use down for things like children, requires, owned things, etc
    std::vector<NodeRelation> down;
    // All nodes may have an assigned UUID. This UUID is declared
    // in this class but is not populated upon creation of the object,
    // as I don't necessarily want to pay the overhead for one every
    // time I create a node. You need boost 1.86 or later for
    // UUID V7 UUIDs (time_generator_7)
    boost::uuids::uuid id;
    
    // Track that information in this node changed. This could
    // be caused by adding something to an up or down list,
    // calling init() to set the UUID or changing a data field
    // in one of the children nodes.
    bool changed = false;
    // Track if the node has been initted. I could just check the
    // UUID for this but setting a bool when init is called is a bit
    // easier.
    bool initted = false;

    Node() {
      // Set UUID now. You can call this again later or use
      // setUuid later if you want to, but all nodes should
      // be inittted at some point, so now seems good.
      init();
    }
    
    // Note: Copying a node will copy its UUID, you may want to
    // rerun init() on the copy if you want it to be a different
    // entity with the same up/down lists.
    Node(const Node& copy) = default;
    virtual ~Node() = default;
    
    // Set the id field
    virtual void init() {
      std::lock_guard<std::mutex> lock(nodeMutex);
      changed = true;
      initted = true;
      boost::uuids::time_generator_v7 generator;
      id = generator();
    }

    // Find a node ID in a vector. Returns a copy because
    // you can remove an item from the list at any time.
    std::optional<NodeRelation> findIn(const std::string& id, std::vector<NodeRelation> &list) {
      for (auto item : list) {
        if (id == item.first->idString()) {
          return std::optional<NodeRelation>(item);
        }
      }
      return std::nullopt;
    }

    // Find a node ID in our uplist
    std::optional<NodeRelation> findUp(const std::string& id) {
      return findIn(id, up);
    }

    // Find a nide in our downlist
    std::optional<NodeRelation> findDown(const std::string& id) {
      return findIn(id, down);
    }

    // findUp but I just want the pointer and not the relation
    PtrType findUpPtr(const std::string& id) {
      PtrType ret;
      auto maybeNode = findUp(id);
      if (maybeNode.has_value()) {
        ret = maybeNode->first;
      }
      return ret;
    }

    PtrType findDownPtr(const std::string& id) {
      PtrType ret;
      auto maybeNode = findDown(id);
      if (maybeNode.has_value()) {
        ret = maybeNode->first;
      }
      return ret;
    }

    // findUp but I just want the Relation and not the pointer
    Relationship findUpRelationship(const std::string& id) {
      Relationship ret = Relationship::Unknown;
      auto maybeNode = findUp(id);
      if (maybeNode.has_value()) {
        ret = maybeNode->second;
      }
      return ret;
    }

    // findDown but I just wantt he relation and not the pointer
    Relationship findDownRelationship(const std::string& id) {
      Relationship ret = Relationship::Unknown;
      auto maybeNode = findDown(id);
      if (maybeNode.has_value()) {
        ret = maybeNode->second;
      }
      return ret;
    } 

    // Add a node to a vector
    void addNode(PtrType node, std::vector<NodeRelation> &list, Relationship tag = Relationship::GraphConnection) {
      if (!findIn(node->idString(), list)) {
        list.push_back(std::make_pair(node, tag));
      }
    }

    // Add a node to our uplist
    void addUp(PtrType node, Relationship tag = Relationship::GraphConnection) {
      addNode(node, up, tag);
    }

    // Add a node to our downlist
    void addDown(PtrType node, Relationship tag = Relationship::GraphConnection) {
      addNode(node, down, tag);
    }

    // Remove a node from a vector
    void removeFromList(PtrType node, std::vector<NodeRelation>& vec) {
      vec.erase(std::remove_if(vec.begin(),
                               vec.end(),
                               [&node](NodeRelation n){
                                 return n.first->idString() == node->idString();
                               }),
                vec.end());
    }

    // Traverse the graph from this node. Pass traverse a lambda to be run
    // against each visited node.

    virtual void traverse(std::function<void(PtrType)> eachNodeFn) {
      std::unordered_map<std::string,PtrType> visited;

      traverse(eachNodeFn, visited);
    }
    
    // Remove a node from the up list
    void removeUp(PtrType node) {
      return removeFromList(node, up);
    }
    
    void removeDown(PtrType node) {
      return removeFromList(node, down);
    }
    
    // Returns ID as string. Note init must be called to actually
    // set the node.
    std::string idString() const {
      std::string ret = boost::uuids::to_string(id);
      return ret;
    }

    // Set UUID from string -- Database load needs this.

    void setUuid(const std::string &uuid) {
      boost::uuids::string_generator generator;
      id = generator(uuid);
      changed = true;
    }

  };
  
}
