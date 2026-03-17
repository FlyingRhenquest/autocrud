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
#include <fr/autocrud/Node.h>

TEST(NodeTests, NodeBasic) {

  fr::autocrud::Node knew;
  // Fortunately we don't use new anymore, so I won't
  // be newing Node knew.
  knew.init();
  ASSERT_FALSE(knew.id.is_nil());
}

TEST(NodeTests, AddUpDownGenericRelations) {

  auto one = std::make_shared<fr::autocrud::Node>();
  // These should auto-init now
  ASSERT_FALSE(one->id.is_nil());
  auto two = std::make_shared<fr::autocrud::Node>();
  auto three = std::make_shared<fr::autocrud::Node>();
  one->addDown(two);
  two->addUp(one);

  two->addDown(three);
  three->addUp(two);

  auto oneRef = two->findUp(one->idString());
  ASSERT_TRUE(oneRef.has_value() && oneRef->first != nullptr);
  ASSERT_EQ(oneRef->first->idString(), one->idString());

  auto threeRef = two->findDown(three->idString());
  ASSERT_TRUE(threeRef.has_value() && threeRef->first != nullptr);
  ASSERT_EQ(threeRef->first->idString(), three->idString());
  
}
