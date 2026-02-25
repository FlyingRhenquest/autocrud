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

// node-specific CRUD tests

TEST(NodeCrud, CRUD) {
  auto node = std::make_shared<fr::autocrud::Node>();
  node->init();
  fr::autocrud::Crud<fr::autocrud::Node> crud;
  pqxx::connection connection;
  
  crud.CreateTable(connection);
  ASSERT_FALSE(crud.Exists(node, connection));
  crud.Create(node, connection);
  ASSERT_TRUE(crud.Exists(node, connection));
  crud.Delete(node, connection);
  ASSERT_FALSE(crud.Exists(node, connection));
}

