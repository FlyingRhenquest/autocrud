/**
 * Copyright 2026 Bruce Ide
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

#include <format>
#include <fr/autocrud/RelationTypes.h>
#include <fr/autocrud/Crud.h>
#include <stdexcept>
#include <span>
#include <map>
#include <memory>
#include <meta>
#include <tuple>
#include <vector>

namespace fr::autocrud {

  /**
   * An object that can do graph CRUD operations on a graph of nodes. It
   * must be provided an array of std::meta::info objects, one for each
   * type in your graph. Any types in your graph that are not listed
   * in the array of NodeTypes will degrade to plain old nodes; they'll
   * still have node associations but they'll have no other data
   * associated with them.
   *
   * Create and Update have been combined in Save; Save will check
   * to see if a node ID exists prior to attempting to save it.
   * If the node ID exists, Save will update that node. Otherwise
   * it will create a new one.
   */

  template<std::array NodeTypes>
  class Graph {    

    // Pair for ReadAssociations -- consists of a string "up" or "down" and a
    // Relationship enum value.
    
    using AssociationPair = std::pair<std::string, Relationship>;
    
    std::string findNodeType(std::string nodeId, pqxx::connection &c) {
      std::string ret;
      std::string cmd = "select node_type from node where id = $1;";
      pqxx::params p{
        nodeId
      };
      pqxx::work work(c);
      pqxx::result res = work.exec(cmd, p);
      if (res.size() > 0) {
        for (auto const &row : res) {
          ret = row["node_type"].as<std::string>();
        }
      }
      return ret;
    }    

    // Read associations for "up" or "down" for a given ID
    void readAssociations(std::string id,
                          std::vector<AssociationPair>& ids,
                          std::string upDown,
                          pqxx::connection &c) {
      std::string cmd = "select association, relationship from node_associations where id = $1 and type = $2;";
      pqxx::params p{
        id,
        upDown
      };
      pqxx::work work(c);
      pqxx::result res = work.exec(cmd, p);
      if (res.size() > 0) {
        for (auto const &row : res) {
          std::string association = row["association"].as<std::string>();
          std::string relationship = row["relationship"].as<std::string>();
          // This will always return a value for a relationship. If it's "Unknown",
          // there might be some sort of error condition going on. It'd likely be
          // a Relationship enum version mismatch.
          auto maybeRelationship = StringToEnum<Relationship>(relationship);
          // But I'm going to explicitly value_or it here too because I'm paranoid
          // about such things.
          ids.push_back(std::make_pair(association, maybeRelationship.value_or(Relationship::Unknown)));
        }
      }
      return;
    }

    // Allocate a node of a specific type
    Node::PtrType allocateNodeType(std::string type) {
      template for (constexpr auto NodeType : NodeTypes) {
        if (type == std::meta::identifier_of(NodeType)) {
          using ActualType = [: NodeType :];
          return std::make_shared<ActualType>();
        }
      }
      // Fall back to Node
      return std::make_shared<Node>();
    }
    
    void Load(Node::PtrType graph, pqxx::connection &c, std::map<std::string, Node::PtrType>& alreadyLoaded) {
      std::string nodeType = findNodeType(graph->idString(), c);
      // Match this against NodeTypes
      template for (constexpr auto NodeType : NodeTypes) {
        if (nodeType == std::meta::identifier_of(NodeType)) {
          // Load current node
          using ActualType = [: NodeType :];
          Crud<ActualType> crud;
          auto actualPtr = std::dynamic_pointer_cast<ActualType>(graph);
          // Sanity check throw
          if (nullptr == actualPtr) {
            std::string errorText = std::format("graph is not a {}", nodeType);
            throw std::logic_error(errorText);
          }
          crud.Read(actualPtr, c);
          alreadyLoaded[graph->idString()] = graph;
          break;
        }
      }
      // Check to see if we saved this node and fallback to Node type if we didn't
      if (!alreadyLoaded.contains(graph->idString())) {
        Crud<Node> crud;
        crud.Read(graph, c);
        alreadyLoaded[graph->idString()] = graph;
      }
      // Handle "up" associations
      std::vector<AssociationPair> ups;
      readAssociations(graph->idString(), ups, "up", c);

      // TODO: Would it be worth turning AssociationPair into a tuple, storing
      // "Up" and "Down" in the tuple with the association Id and relationship,
      // doing both these queries, joining the vector of tuples into one big
      // vector and only writing it once? I kind of like the current simplicity,
      // but this could lead to errors if I'm not careful. Definitely do this
      // if I decide I need to repeat this loop one more time.
      
      // Create and load nodes for each one
      for (auto up : ups) {
        if (alreadyLoaded.contains(up.first)) {
          // Add it to the graph here
          graph->addUp(alreadyLoaded[up.first]);
          // Don't load it again
          continue;
        }
        // Find the node type based on the string portion of AssociationPair
        std::string nodeType = findNodeType(up.first, c);
        Node::PtrType node = allocateNodeType(nodeType);
        node->setUuid(up.first);
        // Add the node to the up list with the Relationship portion of AssociationPair
        graph->addUp(node, up.second);
        Load(node, c, alreadyLoaded);
      }
      // Same for downs
      std::vector<AssociationPair> downs;
      readAssociations(graph->idString(), downs, "down", c);
      for (auto down : downs) {
        if (alreadyLoaded.contains(down.first)) {
          // Add it to the graph here
          graph->addDown(alreadyLoaded[down.first]);
          // Don't load it again.
          continue;
        }
        // Find the node type based on the string portion of AssociationPair (again)
        std::string nodeType = findNodeType(down.first, c);
        Node::PtrType node = allocateNodeType(nodeType);
        node->setUuid(down.first);
        // Add the node to the down list with the Relationship portion of the AssociationPair
        graph->addDown(node, down.second);
        Load(node, c, alreadyLoaded);
      }
    }
    
  public:

    Graph() {}


    /**
     * Saves an entire graph pointed to by Node. Any types not listed in NodeTypes
     * will be stored as a plain "Node" which will have associations in the graph
     * but no other data associated with it.
     */
    
    void Save(Node::PtrType graph, pqxx::connection &c) {
      graph->traverse([&](Node::PtrType node) {
        template for (constexpr auto NodeType : NodeTypes) {
          // See if the NodeType we're looking at matches the current node we're
          // trying to save
          using ThisNodeType = [: NodeType :];
          auto tryNode = std::dynamic_pointer_cast<ThisNodeType>(node);        
          if (tryNode) {
            // Retrieve the Crud type for it
            Crud<ThisNodeType> saver;
            // Check to see if node exists
            if (saver.Exists(tryNode, c)) {
              // Update if it does
              saver.Update(tryNode,c);
            } else {
              // Create new record if it does not
              saver.Create(tryNode,c);
            }
            // Our work for this node is complete, exit the lambda now
            return;
          }          
        }
        // If we haven't returned yet, this is either an unknown node type or
        // a raw Node
        Crud<Node> saver;
        if (saver.Exists(node, c)) {
          saver.Update(node, c);
        } else {
          saver.Create(node,c);
        }
      });
    }

    /**
     * Deletes an entire graph. Mostly used for cleaning up after my
     * integration tests.
     */

    void Delete(Node::PtrType graph, pqxx::connection &c) {
      graph->traverse([&](Node::PtrType node) {
       template for (constexpr auto NodeType : NodeTypes) {
          // See if the NodeType we're looking at matches the current node we're
          // trying to save
          using ThisNodeType = [: NodeType :];
          auto tryNode = std::dynamic_pointer_cast<ThisNodeType>(node);        
          if (tryNode) {
            // Retrieve the Crud type for it
            Crud<ThisNodeType> deleter;

            deleter.Delete(tryNode, c);

            // Our work for this node is complete, exit the lambda now
            return;
          }
        }
       // plain node fallback
       Crud<Node> deleter;
       deleter.Delete(node, c);
      });           
    }

    /**
     * Loads an entire graph. You DO need to make sure the node you pass
     * initially is the correct node type for the UUID associated with it.
     */

    void Load(Node::PtrType graph, pqxx::connection &c) {
      std::map<std::string, Node::PtrType> alreadyLoaded;
      Load(graph, c, alreadyLoaded);
    }

    /**
     * Create all the tables for this Graph
     */
    
    void CreateTables(pqxx::connection &c) {
      template for (constexpr auto NodeType : NodeTypes) {
        using ThisNodeType = [: NodeType :];
        Crud<ThisNodeType> table;
        table.CreateTable(c);
      }
    }

    /**
     * Drop all the tables for this graph. Mostly used for my integration test
     * cleanup.
     */

    void DropTables(pqxx::connection &c) {
      template for (constexpr auto NodeType : NodeTypes) {
        using ThisNodeType = [: NodeType :];
        Crud<ThisNodeType> table;
        table.DropTable(c);
      }
    }
    
  };
  
}
