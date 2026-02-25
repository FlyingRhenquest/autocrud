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

/**
 * Integration tests write into a database and drops tables from it.
 * You should not run them against any database instance that anyone
 * else is trying to work out of. The safest bet is to create a
 * separate database instance to run these tests, possibly even
 * in an isolated docker VM or something. The CMake instrumentation
 * will warn you about this again, when you enable the
 * BUILD_INTEGRATION_TESTS flag. I'll still try to keep it as
 * safe as possible and won't drop the node or node_associations
 * table in these tests.
 */

#include <gtest/gtest.h>
#include <fr/autocrud/Crud.h>
#include <memory>
#include <pqxx/pqxx>

// Let's start with a basic node

TEST(Integration, CreateTableAndWriteNode) {
  pqxx::connection c;
  // Still not newing Node knew
  fr::autocrud::Node::PtrType knew = std::make_shared<fr::autocrud::Node>();
  
  fr::autocrud::Crud<fr::autocrud::Node> table;
  // This is safe to do, as it will only create the table if it
  // doesn't exist
  table.CreateTable(c);
  // Since I just created this node, I know it doesn't
  // exist in the database yet.
  // May as well assert on it anyway.
  ASSERT_FALSE(table.Exists(knew, c));
  table.Create(knew,c);
  ASSERT_TRUE(table.Exists(knew, c));
  table.Delete(knew,c);
  // Not gonna delete node/node_associations though
}

// Make sure node associations work
TEST(Integration, NodeAssociationsBasic) {
  pqxx::connection c;

  fr::autocrud::Node::PtrType one = std::make_shared<fr::autocrud::Node>();
  fr::autocrud::Node::PtrType two = std::make_shared<fr::autocrud::Node>();
  fr::autocrud::Node::PtrType three = std::make_shared<fr::autocrud::Node>();

  // Add up/down are not reciprocal by design. You can absolutely
  // have a one-way link to another node and that's fine. You just
  // need to be careful what node you load stuff from if you have
  // a graph that actually does this, as recursive loads won't get
  // the entire graph if you try to load from a node that doesn't
  // link back to other graph nodes.
  one->addUp(two);
  one->addDown(three);
  two->addDown(one);
  three->addUp(one);

  fr::autocrud::Crud<fr::autocrud::Node> table;
  table.CreateTable(c);

  // Gotta write 'em all!
  one->traverse([&table,&c](auto nodePtr) {
    table.Create(nodePtr, c);
  });

  ASSERT_TRUE(table.Exists(one, c));
  ASSERT_TRUE(table.Exists(two, c));
  ASSERT_TRUE(table.Exists(three, c));

  table.Delete(one,c);
  table.Delete(two,c);
  table.Delete(three,c);
    
}

// How about Derived Nodes?
TEST(Integration, DerivedNodeBasicOneNode) {

  struct Derived : public fr::autocrud::Node {
    [[=fr::autocrud::DbFieldName{std::define_static_string("foo")}]] std::string pleh;
    int bar;
    int baz;
  };

  pqxx::connection c;

  auto node = std::make_shared<Derived>();
  ASSERT_TRUE(node->initted);

  fr::autocrud::Crud<Derived> table;
  table.CreateTable(c);

  node->pleh = "Florble";
  node->bar = 24;
  node->baz = 42;

  table.Create(node, c);
  ASSERT_TRUE(table.Exists(node, c));

  // Read it back
  auto copy = std::make_shared<Derived>();
  // We have to set the ID for Read to find
  // the record
  copy->id = node->id;
  ASSERT_TRUE(table.Read(copy, c));
  ASSERT_EQ(node->idString(), copy->idString());
  ASSERT_EQ(node->pleh, copy->pleh);
  ASSERT_EQ(node->bar, copy->bar);
  ASSERT_EQ(node->baz, copy->baz);

  // How about Update?
  node->baz = 12345;
  node->pleh = "PLEH!";

  table.Update(node,c);
  table.Read(copy, c);

  ASSERT_EQ(node->idString(), copy->idString());
  ASSERT_EQ(node->pleh, copy->pleh);
  ASSERT_EQ(node->bar, copy->bar);
  ASSERT_EQ(node->baz, copy->baz);
  
  table.Delete(node,c);
  table.DropTable(c);
  
}
