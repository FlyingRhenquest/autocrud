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

#include <array>
#include <gtest/gtest.h>
#include <fr/autocrud/Crud.h>
#include <fr/autocrud/Graph.h>
#include <fr/autocrud/Helpers.h>
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

/**
 * Verify Graph works. We'll include a plain node,
 * a couple of derived nodes and a derived node that we
 * don't inform our Graph of. The derived node we
 * don't inform our Graph of should be stored as
 * a plain old Node.
 */
TEST(Integration, Graph) {

  struct UserName : public fr::autocrud::Node {
    [[="VARCHAR(40)"_ColumnType]] std::string first_name;
    [[="VARCHAR(40)"_ColumnType]] std::string last_name;
  };

  struct PhoneNumber : public fr::autocrud::Node {
    [[="VARCHAR(15)"_ColumnType]] std::string number;
  };

  struct Address : public fr::autocrud::Node {
    std::string address;
  };

  constexpr std::array NodeTypes = {^^UserName, ^^PhoneNumber};
  fr::autocrud::Graph<NodeTypes> saver;
  pqxx::connection c;

  saver.CreateTables(c);

  auto graph = std::make_shared<fr::autocrud::Node>();
  auto name = std::make_shared<UserName>();
  name->first_name = "Some";
  name->last_name = "Rando";
  // Attach name to graph
  graph->addDown(name);
  name->addUp(graph);

  auto phone = std::make_shared<PhoneNumber>();
  phone->number = "5551212";
  graph->addDown(phone);
  phone->addUp(graph);

  auto address = std::make_shared<Address>();
  address->address = "123 Rando Street";
  graph->addDown(address);
  address->addUp(graph);

  saver.Save(graph,c);

  // We'll use a regular node Crud object to verify
  // add Ids in the graph were stored in the database

  fr::autocrud::Crud<fr::autocrud::Node> validator;

  // Note that although this passes, the Address information has in
  // fact been lost because we (deliberately) did not include Address
  // in NodeTypes. The address node ID still exsists, but no table
  // was created for Address.
  
  graph->traverse([&](fr::autocrud::Node::PtrType node) {
    ASSERT_TRUE(validator.Exists(node,c));
  });

  // Load graph from database
  auto copy = std::make_shared<fr::autocrud::Node>();
  copy->setUuid(graph->idString());

  saver.Load(copy, c);

  bool foundPhoneNumber = false;
  bool foundUserName = false;

  copy->traverse([&](fr::autocrud::Node::PtrType node) {
    auto copyName = std::dynamic_pointer_cast<UserName>(node);
    if (copyName) {
      ASSERT_EQ(copyName->first_name, "Some");
      ASSERT_EQ(copyName->last_name, "Rando");
      foundUserName = true;
    }
    auto copyPhone = std::dynamic_pointer_cast<PhoneNumber>(node);
    if (copyPhone) {
      ASSERT_EQ(copyPhone->number, "5551212");
      foundPhoneNumber = true;
    }
  });

  ASSERT_TRUE(foundPhoneNumber);
  ASSERT_TRUE(foundUserName);

  // Clean up after test
  saver.Delete(graph,c);
  saver.DropTables(c);
}

// Verify a recursive graph does not load infinitely
TEST(Integration, Recursive) {

  struct Recursive : public fr::autocrud::Node {
    [[="VARCHAR(12)"_ColumnType]] std::string label;
  };

  auto recursive = std::make_shared<Recursive>();
  recursive->label = "Recursive!";

  // This is fine...
  recursive->addUp(recursive);
  recursive->addDown(recursive);

  pqxx::connection c;
  // I could do this with a raw crud but we're testing graph here
  constexpr std::array NodeTypes = { ^^Recursive };
  fr::autocrud::Graph<NodeTypes> saver;
  saver.CreateTables(c);
  saver.Save(recursive, c);

  // We will use a Crud for validation though
  fr::autocrud::Crud<Recursive> validator;
  ASSERT_TRUE(validator.Exists(recursive, c));

  auto copy = std::make_shared<Recursive>();
  copy->setUuid(recursive->idString());
  saver.Load(copy, c);

  ASSERT_EQ(copy->label, recursive->label);
  auto copyUp = copy->findUp(copy->idString());
  ASSERT_TRUE(copyUp);
  ASSERT_EQ(copy->idString(), copyUp->idString());
  auto copyDown = copy->findDown(copy->idString());
  ASSERT_TRUE(copyDown);
  ASSERT_EQ(copy->idString(), copyDown->idString());  
}
