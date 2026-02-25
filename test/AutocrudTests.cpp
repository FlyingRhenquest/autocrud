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

#include <gtest/gtest.h>
#include <fr/autocrud/Crud.h>
#include <fr/autocrud/Node.h>

TEST(Autocrud, Basic) {

  // This is the Node-specialized version
  fr::autocrud::Crud<fr::autocrud::Node> nodeCrud;

  ASSERT_EQ(nodeCrud.tableName, std::string("Node"));
  
}

/**
 * Verify table definitions get created in Crud for
 * objects derived from Node
 */

TEST(Autocrud, TableDefBasic) {

  struct Child : public fr::autocrud::Node {
    int foo; // Should be BIGINT
    std::string bar; // should be TEXT
    [[=fr::autocrud::DbIgnore{}]] float ignore;
    // Force baz to "VARCHAR(100)" rather than using "TEXT", which is what string
    // is defined to in CrudTypes.h
    [[=fr::autocrud::DbFieldType{std::define_static_string("VARCHAR(100)")}]] std::string baz;
    // Rename definitelyNotSteve to "steve" in the database.
    [[=fr::autocrud::DbFieldName{std::define_static_string("steve")}]] std::string definitelyNotSteve;
  };

  fr::autocrud::Crud<Child> crud;
  auto child = std::make_shared<Child>();
  
  child->foo = 42;
  child->bar = "PLEH!";
  child->ignore = 3.33;
  child->baz = "BAZ!";
  child->definitelyNotSteve = "No, it's Steve.";

  // This gets done in the Node constructor
  ASSERT_TRUE(child->initted);
  
  // We'll do these in their own scope to keep variable names tidy
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<0>();
    ASSERT_EQ(cppName, std::string_view("foo"));
    ASSERT_EQ(dbName, std::string_view("foo"));
    ASSERT_EQ(dbType, std::string_view("BIGINT"));
    ASSERT_EQ((*child).*ptr, 42);
  }
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<1>();
    ASSERT_EQ(cppName, std::string_view("bar"));
    ASSERT_EQ(dbName, std::string_view("bar"));
    ASSERT_EQ(dbType, std::string_view("TEXT"));
    ASSERT_EQ((*child).*ptr, "PLEH!");
  }
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<2>();
    // All strings in the tuple will be empty for an ignore field
    // and ptr will be nullptr, so if you're using this test interface
    // to do something else with your column names, you'd best check
    // the lengths before using anything in there.
    ASSERT_EQ(strlen(cppName), 0);
  }
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<3>();
    ASSERT_EQ(cppName, std::string_view("baz"));
    ASSERT_EQ(dbName, std::string_view("baz"));
    ASSERT_EQ(dbType, std::string_view("VARCHAR(100)"));
    ASSERT_EQ((*child).*ptr, "BAZ!");
  }
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<4>();
    ASSERT_EQ(cppName, std::string_view("definitelyNotSteve"));
    ASSERT_EQ(dbName, std::string_view("steve"));
    ASSERT_EQ(dbType, std::string_view("TEXT"));
    ASSERT_EQ((*child).*ptr, "No, it's Steve.");
  }
  // Yay! We can introspect our class as expected.
}
