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
#include <fr/autocrud/Index.h>
#include <fr/autocrud/Node.h>
#include <fr/autocrud/VectorType.h>

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
  ASSERT_EQ(std::string(crud.tableName), "Child");
  
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
    const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<0>();
    ASSERT_EQ(cppName, std::string_view("foo"));
    ASSERT_EQ(dbName, std::string_view("foo"));
    ASSERT_EQ(dbType, std::string_view("BIGINT"));
    ASSERT_EQ((*child).*ptr, 42);
  }
  {
    const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<1>();
    ASSERT_EQ(cppName, std::string_view("bar"));
    ASSERT_EQ(dbName, std::string_view("bar"));
    ASSERT_EQ(dbType, std::string_view("TEXT"));
    ASSERT_EQ((*child).*ptr, "PLEH!");
  }
  {
    const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<2>();
    // Ignore field names will be nullptr
    ASSERT_EQ(cppName, nullptr);
  }
  {
    const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<3>();
    ASSERT_EQ(cppName, std::string_view("baz"));
    ASSERT_EQ(dbName, std::string_view("baz"));
    ASSERT_EQ(dbType, std::string_view("VARCHAR(100)"));
    ASSERT_EQ((*child).*ptr, "BAZ!");
  }
  {
    const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<4>();
    ASSERT_EQ(cppName, std::string_view("definitelyNotSteve"));
    ASSERT_EQ(dbName, std::string_view("steve"));
    ASSERT_EQ(dbType, std::string_view("TEXT"));
    ASSERT_EQ((*child).*ptr, "No, it's Steve.");
  }
  // Yay! We can introspect our class as expected.
}

// Make sure VectorType maps correctly;
TEST(Autocrud, VectorType) {
  struct VectorTable : public fr::autocrud::Node {
    fr::autocrud::Vector<1536> embedding;
  };

  fr::autocrud::Crud<VectorTable> crud;

  {
    const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<0>();
    ASSERT_EQ(cppName, std::string_view("embedding"));
    ASSERT_EQ(dbName, std::string_view("embedding"));
    ASSERT_EQ(dbType, std::string_view("public.vector(1536)"));
  }
}

TEST(Autocrud, RenameTable) {
  // Would recommend generally using the helper for this (See HelpersTest.cpp)
  struct [[=fr::autocrud::DbTableName(std::define_static_string("generic_table"))]]
    NotAGenericTable : public fr::autocrud::Node {
    int foo;
  };

  fr::autocrud::Crud<NotAGenericTable> crud;
  ASSERT_EQ(std::string(crud.tableName), "generic_table");
}

TEST(Autocrud, IndexNoDefaults) {
  struct IndexedTable : public fr::autocrud::Node {
    // Index will apply to thing. You can apply multiple indexes to
    // a single column.
    [[= fr::autocrud::Index {
          .Name = std::define_static_string("thing_index"),
          .On = std::define_static_string("IndexedTable"),
          .Using = std::define_static_string("GIN (thing)"),
          .Where = std::define_static_string("thing = 'monkey'")}
        ]]
    [[= fr::autocrud::Index {
          .Name = std::define_static_string("thing_index_2"),
          .On = std::define_static_string("IndexedTable"),
          .Using = std::define_static_string("GIN (thing)"),
          .Where = std::define_static_string("thing = 'bagel'")}
        ]]
    std::string thing;
  };

  fr::autocrud::Crud<IndexedTable> crud;
  const auto [cppName, dbName, dbType, indexes, ptr] = crud.column<0>();

  ASSERT_EQ(indexes.index[0].Name, std::string_view("thing_index"));
  ASSERT_EQ(indexes.index[0].On, std::string_view("IndexedTable"));
  ASSERT_EQ(indexes.index[0].Using, std::string_view("GIN (thing)"));
  ASSERT_EQ(indexes.index[0].Where, std::string_view("thing = 'monkey'"));
  ASSERT_EQ(indexes.index[1].Name, std::string_view("thing_index_2"));
  ASSERT_EQ(indexes.index[1].On, std::string_view("IndexedTable"));
  ASSERT_EQ(indexes.index[1].Using, std::string_view("GIN (thing)"));
  ASSERT_EQ(indexes.index[1].Where, std::string_view("thing = 'bagel'"));
}

/**
 * Make sure a vector can query an embedding engine to get embeddings for
 * a piece of text.
 */

#ifdef ENABLE_EMBEDDING_ENGINE_TESTS

TEST(Autocrud, VectorEmbeddingEngine) {
  std::string modelPath{MODEL_LOC}; // Passed in from CMake
  fr::autocrud::Vector<64> embeddings;
  auto engine = std::make_shared<fr::autocrud::EmbeddingEngine>(modelPath);
  embeddings.setEngine(engine);
  ASSERT_FALSE(embeddings.hasEmbeddings());
  ASSERT_EQ(embeddings.size(), 0l);
  embeddings.setEmbeddings("The quick brown fox something something danger zone");
  ASSERT_TRUE(embeddings.hasEmbeddings());
  std::cout << embeddings.toPgString() << std::endl;
}

#endif
