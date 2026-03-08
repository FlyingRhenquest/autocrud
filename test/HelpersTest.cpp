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
#include <fr/autocrud/Helpers.h>
#include <string.h>

TEST(Helpers, annotations) {

  struct Derived : public fr::autocrud::Node {
    [[=Ignore() ]] int ignored;
    [[="VARCHAR(100)"_ColumnType]] std::string foo;
    [[="quux"_ColumnName]] std::string bar; // will be named quux
  };

  fr::autocrud::Crud<Derived> crud;

  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<0>();
    // "ignored" ignored successfully
    ASSERT_EQ(cppName, nullptr);
  }
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<1>();
    ASSERT_EQ("foo", std::string(cppName));
    ASSERT_EQ("foo", std::string(dbName));
    ASSERT_EQ("VARCHAR(100)", std::string(dbType));
  }
  {
    const auto [cppName, dbName, dbType, ptr] = crud.column<2>();
    ASSERT_EQ("bar", std::string(cppName));
    ASSERT_EQ("quux", std::string(dbName));
    ASSERT_EQ("TEXT", std::string(dbType));
  }
  
}
