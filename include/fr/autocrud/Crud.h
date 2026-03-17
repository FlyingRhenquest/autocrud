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

#include <concepts>
#include <fr/autocrud/CrudTypes.h>
#include <fr/autocrud/RelationTypes.h>
#include <fr/autocrud/Node.h>
#include <pqxx/pqxx>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <memory>
#include <meta>
#include <map>
#include <ranges>

namespace fr::autocrud {

  /**
   * DbIgnore is a type you can put in an annotation to signal to autocrud
   * that you do not want to generate a database field for that member.
   */

  struct DbIgnore {
  };
  
  /**
   * DbFieldType is a type you can put in an annotation to signal to autocrud
   * that you want a field to be of the type you specify
   *
   * ex: struct pleh {
   *        [[=DbFieldType(std::define_static_string("VARCHAR(40))"]] std::string lastName;
   * };
   */

  struct DbFieldType {
    char const* fieldType;
  };

  /**
   * DbFieldName is a type you can put in an annotation to signal to autocrud
   * that you want a field to have a specific name.
   *
   * ex: struct pleh {
   *         [[=DbFieldName(std::define_static_string("last_name"))]] std::string lastName;
   * };
   */

  struct DbFieldName {
    char const* fieldName;
  };

  /**
   * DbTableName is a type you can put in an annotation to a struct to signal
   * autocrud that you want the table to have a specific name.
   *
   * ex: struct [[=DbTableName(std::define_static_string("user_name"))]] UserName
   */

  struct DbTableName {
    char const* tableName;
  };

  // Predeclare crud
  template <typename T>
  requires std::derived_from<T, Node>
  struct Crud;

  //------------------------------ Crud (Node Specialization) ------------------------------
  
  /**
   * Node does require special handling, as I want to write its up and
   * down lists to node_associations. So here's the node-specific specialization
   */

  template <>
  struct Crud<Node> {
    using Type = Node;
    static constexpr char tableName[] = "Node";

    Crud() {}
    
    /**
     * Create the table if it doesn't exist
     */
    
    void CreateTable(pqxx::connection &c) {
      pqxx::work work(c);
      // create node table
      std::string CNTable = std::format("CREATE TABLE IF NOT EXISTS {} (", tableName);
      CNTable.append("id         UUID PRIMARY KEY,");
      CNTable.append("node_type  VARCHAR(100) NOT NULL);");
      // Define line for relationship
      std::string relationshipLine = std::format("relationship    VARCHAR({}) NOT NULL,", MAX_ENUM_STRING_LENGTH);
      // create assocations table      
      std::string CATable("CREATE TABLE IF NOT EXISTS node_associations (");
      CATable.append("id          UUID NOT NULL,");
      CATable.append("association UUID NOT NULL,");
      CATable.append(relationshipLine);
      CATable.append("type        VARCHAR(5) NOT NULL);"); // "Will be "up" or "down"
      work.exec(CNTable);
      work.exec(CATable);
      work.commit();
    }

    /**
     * Drop table - This will drop your table. Should probably generally only
     * be used for cleaning up after integration tests.
     */

    void DropTable(pqxx::connection &c) {
      pqxx::work work(c);
      std::string dropAssociations("DROP TABLE node_associations;");
      work.exec(dropAssociations);
      std::string drop = std::format("DROP TABLE {};", tableName);
      work.exec(drop);
      work.commit();
    }

    /**
     * Check if the node is in the database -- this dictates whether we save
     * with Create or Update.
     */
    
    bool Exists(Node::PtrType n, pqxx::connection& c) {
      pqxx::work work(c);
      bool ret = false;
      std::string cmd = std::format("select id from {} where ID = $1;", tableName);
      pqxx::params p{
        n->idString()
      };

      pqxx::result res = work.exec(cmd, p);
      if (res.size() > 0) {
        ret = true;
      }
      return ret;
    }
    
    /**
     * Crud<Node>::Create is a 2-parter. If the record doesn't exist, we need to create
     * it in the node table, which is just id and type. We also need to update the node
     * associations. Update will always need to update the node associations, but does not
     * need to create the record, since if update is getting called there is already a
     * record in node.
     *
     * So we insert into the node table with create and then call update to update
     * the node associations. That will consistently do the right thing (citation needed)
     * down here at the bottom of the inheritance tree.
     */
    void Create(Node::PtrType n, pqxx::connection& c, const std::string nodeType = "Node") {
      // Force uuid assignment now if necessary
      if (!n->initted) {
        n->init();
      }
      pqxx::work work(c);
      std::string cmd = std::format("INSERT INTO {} (id, node_type) VALUES($1, $2);", tableName);
      pqxx::params p{
        n->idString(),
        nodeType
      };
      work.exec(cmd,p);
      work.commit();
      Update(n, c);
    }

    /**
     * Reads a node. The ID in the passed-in node must be equal to the ID you want to
     * read for most node types. But this is a no-op in node because we're handling
     * node_associations elsewhere and if you're passing in a node that has its ID
     * set to the node you want to retrieve, you know literally all the data there
     * is to know about the node anyway.
     *
     * Will return Exists, since it's not reading anything.
     */

    bool Read(Node::PtrType n, pqxx::connection& c) {
      return Exists(n, c);
    }
    
    /**
     * Update deletes all the node_associations for this Node and then rewrites them.
     * This will keep the database from getting out of synch with the object, since
     * you can dynamically add and remove node associations at any time
     */
    void Update(Node::PtrType n, pqxx::connection& c) {
      pqxx::work work(c);
      // We don't need to delete where association = $1, as that would delete pointers
      // to this node
      std::string rmAssociations("DELETE FROM node_associations WHERE id = $1;");
      pqxx::params p{
        n->idString()
      };
      work.exec(rmAssociations, p);
      pqxx::stream_to stream = pqxx::stream_to::table(work, {"public", "node_associations"},
                                                      {"id", "association", "relationship", "type"});
      for (auto up : n->up) {
        // The version of pqxx I'm using doesn't support string_view, so I have to convert to
        // string
        std::string relationship = std::string(EnumToString<Relationship>(up.second));
        stream.write_values(n->idString(), up.first->idString(), relationship, "up");
      }
      for (auto down : n->down) {
        std::string relationship = std::string(EnumToString<Relationship>(down.second));
        stream.write_values(n->idString(), down.first->idString(), relationship, "down");
      }
      stream.complete();
      work.commit();
    }

    /**
     * Removes a node and its associated associations. This could screw up your graph
     * if you're not careful, so make sure to adjust surrounding node associatons
     * prior to doing this unless you're deleting an entire graph. This does not delete
     * the node out of memory, but you could unlink all of its node associations manually
     * to be sure it won't be rewritten if you save the graph later. I don't provide
     * functions to do this. Feel free to file a feature request if you need some. They
     * maybe should go in Graph rather than Node, depending on the specific functionality
     * requested.
     */
    
    void Delete(Node::PtrType n, pqxx::connection& c) {
      pqxx::work work(c);
      // Here we do want to delete both id and associations to point to this node,
      // since we're deleting this node
      std::string rmAssociations("DELETE FROM node_associations WHERE id = $1 OR association = $1;");
      std::string rmNode("DELETE FROM Node WHERE id = $1;");
      pqxx::params p{
        n->idString()
      };
      work.exec(rmAssociations, p);
      work.exec(rmNode, p);
      work.commit();
    }
  };
  

  //----------------------------- Crud (Generic) -----------------------------------------------
  
  /**
   * Crud provides functionality to write a single node to a table. It does record
   * the associations in the up/down lists, but it doesn't actually recursively
   * create or load those nodes. This allows you to pluck one node out of the database
   * without loading its entire association graph at the same time.
   *
   * If your object has private members you want to serialize to the database, you'll
   * need to add "friend class Crud<objectype>" to your class definition.
   */

  template <typename T>
  requires std::derived_from<T, Node>
  struct Crud {
    
    using Type = T;
    using PtrType = std::shared_ptr<Type>;
        
  private:
    // Access context for std::meta (We get both public and private fields)
    constexpr static auto _ctx = std::meta::access_context::unchecked();
    // Max members in storage. If we have a DbIgnore annotation, it could be less than this,
    // but it won't be more.
    constexpr static size_t _maxMembers = std::meta::nonstatic_data_members_of(^^T, _ctx).size();    

    /**
     * Returns true if member has an annotation of a specific type
     */
    
    template <typename Annotation, auto Member>
    static consteval bool has_annotation() {
      template for(constexpr auto a : std::define_static_array(std::meta::annotations_of_with_type(Member, ^^Annotation))) {
        // Since we're using "annotations_of_with_type here, if there are any at all we can just return true.
        // No checks required
        return true;
      }
      return false;
    }

    /**
     * Checks for a table name annotation and returns the table name
     */

    static consteval auto TableName() {
      if constexpr(has_annotation<DbTableName, ^^T>()) {
        return std::meta::extract<DbTableName>(std::meta::annotations_of_with_type(^^T, ^^DbTableName)[0]).tableName;
      } 
      return std::define_static_string(std::meta::identifier_of(^^T));
    }
    
    /**
     * Check to see if a field name is overridden with an annotation and return the correct field name.
     */
    
    template <std::meta::info field>
    static constexpr auto FieldName() {
      if constexpr(has_annotation<DbFieldName, field>()) {
        return std::meta::extract<DbFieldName>(std::meta::annotations_of_with_type(field, ^^DbFieldName)[0]).fieldName;
      } else {
        return std::define_static_string(std::meta::identifier_of(field));
      }
    }

    /**
     * Check to see if a field type is overridden with an annotation and return the correct field type. If your
     * C++ type isn't mapping correctly to a database type, you can add a type specialization in
     * fr/CrudTypes.h. That's where TypeToDbType is defined.
     */
    
    template <std::meta::info field>
    static constexpr auto FieldType() {
      if constexpr(has_annotation<DbFieldType, field>()) {
        return std::meta::extract<DbFieldType>(std::meta::annotations_of_with_type(field, ^^DbFieldType)[0]).fieldType;
      } else {
        using MemberType = [:std::meta::type_of(field):];
        return TypeToDbType<MemberType>::fieldType;
      }
    }

    /**
     * Define what our column tuple looks like.
     */
    
    template <typename MemberPtrType>
    using ColumnTuple = std::tuple<
      const char*,
      const char*,
      const char*,
      MemberPtrType>;
    
    /**
    
    // Trying to use "template for" to stamp these out led to a variety of problems, so I'm going to resort to the
    // old-timey "recursively define a tuple" metaprogramming approach.
    
    /**
     * MakeColumnTuple creates a tuple for a field in the database. It accepts a std::info for one field and returns
     * a tuple with the C++ name of the type as a string_view, the database name of the type as a string view,
     * the database type of the type as a string view and a pointer-to-member that can be used to access the
     * member when we need to insert data into the database.
     */

    template <std::meta::info field>
    static constexpr auto MakeColumnTuple() {
        using Ptr = decltype(&[: field :]);
        // C++ name of row (0)
        return ColumnTuple<Ptr> {
          std::define_static_string(std::meta::identifier_of(field)),
          FieldName<field>(),
          // Database type of row (2)
          FieldType<field>(),
          // Member pointer for this row (field) (3)
          &[: field :]
        };
    }

    /**
     * Fold shenanigans to build my tuple structure so it'll decompose correctly across the
     * entire structured.
     */
    template <size_t memberIndex, typename... Accumulator>
    static constexpr auto BuildDef(Accumulator... blob) {
      constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, _ctx));
      
      if constexpr (memberIndex == (fields.size())) {
        return std::tuple<Accumulator...>{blob...};
      } else {
        // build in an ignore tuple
        if constexpr(has_annotation<DbIgnore, fields[memberIndex]>()) {
          using Ptr = decltype(&[: fields[memberIndex] :]);
          return BuildDef<memberIndex + 1>(blob..., ColumnTuple<Ptr>{{}, {}, {}, nullptr});
        } else {
          return BuildDef<memberIndex + 1>(blob..., MakeColumnTuple<fields[memberIndex]>());
        }
      }
    }
    
    /**
     * Construct the tuple we need to get info on our database columns later on. This is
     * just an entrypoint for BuildDef.
     */
    
    static constexpr auto BuildDefs() {
      return BuildDef<0>();
    }

    static constexpr auto _columns = BuildDefs();
      
  public:
    // Will be a DbTableName annotation if one exists OR the identifier of Struct T
    static constexpr auto tableName = TableName();
    static constexpr auto columnsSize = std::tuple_size<decltype(_columns)>{};
    static constexpr auto lastColumn = std::tuple_size<decltype(_columns)>{} - 1;
    
    Crud() {
    }

    /**
     * Return column tuple by index
     *
     * You can convert to structured bindings with
     * auto [cppName, dbName, dbType, ptr] = column<0>();
     *
     * You DO need to check that cppName != nullptr before using
     * it, as that will denote an ignored field and those
     * should be skipped.
     */
    template <size_t index>
    constexpr auto& column() {
      return std::get<index>(_columns);
    }
    
    /**
     * Creates a new table in the database if one doesn't already exist.
     */
    
    void CreateTable(pqxx::connection& c) {
      std::string cmd = std::format("CREATE TABLE IF NOT EXISTS {} (", tableName);
      // All tables will have the UUID id from Node.
      cmd.append("id UUID PRIMARY KEY");
      // Bring in table defs from our columns tuple
      template for (constexpr size_t i : std::views::iota(0, columnsSize)) {
        const auto [cppName, dbName, dbType, ptr] = this->column<i>();
        // This is an ignore field
        if (cppName && 0 == strlen(cppName)) {
          continue;
        };
        cmd.append(std::format(",{} {}", dbName, dbType));
      }
      cmd.append(");");
      pqxx::work work(c);
      work.exec(cmd);
      work.commit();

      // Also call Node::createTable. It's safe to do since it'll only be created
      // if it doesn't exist. It's a little extra work but it's not like you create
      // a table every day.
      Crud<Node> node;
      node.CreateTable(c);
    }

    /**
     * Drops this table. Should probably only ever be used in integration testing
     */

    void DropTable(pqxx::connection& c) {
      pqxx::work work(c);
      std::string drop = std::format("DROP TABLE {};", tableName);
      work.exec(drop);
      work.commit();
    }

    // For all of these you pass in a node with the id set to the id
    // you're looking for. For read/query/delete, you just set the id
    // you want and the record will be looked up in the database based
    // on that.
    
    /**
     * Check if a record exists.
     */

    bool Exists(PtrType n, pqxx::connection& c) {
      pqxx::work work(c);
      bool ret = false;
      std::string cmd = std::format("select id from {} where ID = $1", tableName);
      pqxx::params p{
        n->idString()
      };

      pqxx::result res = work.exec(cmd, p);
      if (res.size() > 0) {
        ret = true;
      }
      return ret;
    }
    
    /**
     * Creates a record if one doesn't already exist.
     */
    
    void Create(PtrType n, pqxx::connection& c) {
      // We'll build these strings in 3 parts
      std::string cmd = std::format("INSERT INTO {}", tableName);
      // Fields always starts with id
      std::string fields(" (id");
      // Values always starts with $1
      std::string values(" VALUES ($1");
      // Params always starts with IDs
      pqxx::params p{
        n->idString()
      };
      template for (constexpr size_t i : std::views::iota(0, columnsSize)) {
        const auto [cppName, dbName, dbType, ptr] = this->column<i>();
        // Skip ignore field
        if (!cppName || 0 == strlen(cppName)) {
          continue;
        }
        fields.append(std::format(",{}", dbName));
        values.append(std::format(",${}", i + 2)); // Because values should be at 2 when I is 0
        // Append the member to save to params
        p.append((*n).*ptr);
      }
      fields.append(")");
      values.append(");");
      cmd.append(fields);
      cmd.append(values);
      pqxx::work work(c);
      work.exec(cmd, p);
      work.commit();            
      // We need to update node table too
      Crud<Node> node;
      node.Create(n, c, tableName);
    }

    /**
     * Read a record found by PtrType->id.
     */

    bool Read(PtrType n, pqxx::connection& c) {
      bool ret = false;
      std::string cmd(std::format("select * from {} where id = $1;", tableName));
      pqxx::params p{
        n->idString()
      };
      pqxx::work work(c);
      pqxx::result res = work.exec(cmd, p);
      if (res.size() > 0) {
        ret = true;
      }
      // Retrieve the row record from res
      for (auto const &row : res) {
        template for (constexpr size_t i : std::views::iota(0, columnsSize)) {
          const auto [cppName, dbName, dbType, ptr] = this->column<i>();
          if (!cppName || 0 == strlen(cppName)) {
            continue;
          }
          // Retrieve the type so we can set it wtih row[string].as<type>()
          using ColumnType = std::decay_t<decltype((*n).*ptr)>;
          // Nothing up my sleeve
          (*n).*ptr = row[std::string(dbName)].template as<ColumnType>();
        }
      }
      return ret;
    }

    /**
     * Updates a record found by PtrType->id
     */

    void Update(PtrType n, pqxx::connection& c) {
      // We we start our $ fields (ID is always 1)
      size_t startAt = 2;
      std::string cmd(std::format("UPDATE {} SET ", tableName));
      pqxx::params p {
        n->idString()
      };
      size_t skippedSome = 0;
      template for (constexpr size_t i : std::views::iota(0, columnsSize)) {
        const auto [cppName, dbName, dbType, ptr] = this->column<i>();
        if (!cppName || 0 == strlen(cppName)) {
          continue;
        }
        if (0 != skippedSome++) {
          cmd.append(", ");
        }
        cmd.append(std::format("{} = ${}", dbName, startAt++));
        p.append((*n).*ptr);
      }
      cmd.append(" WHERE ID = $1;");
      pqxx::work work(c);
      work.exec(cmd, p);
      work.commit();
      // This is fine. If I explicitly create a Crud of the specialized Node type
      // and pass it our pointer for an update, it'll do the node work and
      // update associations correctly.
      Crud<Node> node;
      node.Update(n, c);
    }
    
    /**
     * Delete a record found by PTrType->id
     */
    
    void Delete(PtrType n, pqxx::connection& c) {
      std::string cmd = std::format("DELETE FROM {} WHERE ID = $1;", tableName);
      pqxx::params p {
        n->idString()
      };
      pqxx::work work(c);
      work.exec(cmd, p);
      work.commit();
      // And delete anything about it out of the node and node_associations table too
      Crud<Node> node;
      node.Delete(n, c);
    }
  
  };

}
