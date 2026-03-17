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

#include <meta>
#include <optional>
#include <string>
#include <type_traits>

namespace fr::autocrud {

  /**
   * Max Relation string legth. This will be set for node_associations
   * in Crud object's "CreateTables" -- it will be
   * VARCHAR(MAX_ENUM_STRING_LENGTH). Hopefully this is a sensible
   * default. If you use UTF8 or unicode or something, you may need
   * to tweak it.
   */

  constexpr size_t MAX_ENUM_STRING_LENGTH=64;
  
  /**
   * Relations you can tag up or down nodes with. I put this in a
   * separate file so you can easily modify this enum to suit your
   * needs. Enums will be reflected into strings to be stored in
   * the database. You can arbitrarily tag any up or down
   * relationship in the table with one of these values.
   */

  enum class Relationship {
    GraphConnection,    // Generic graph connection, no special relationship
    Uses,               // Object A uses Object B
    Requires,           // Object A requires Object B
    Provides,           // Object A provides Object B (Object B might be a capability or library)
    Replaces,           // Object A replaces Object B
    ReplacedBy,         // Object A is replaced by Object B
    Conflicts,          // Object A conflicts with object B (XOR relationship)
    Influences,         // Object A influences Object B (Descripts a decision relationship)
    InfluencedBy,       // Object A is influenced by Object B
    Describes,          // Object A describes Object B
    DescribedBy,        // Object A is described by object B
    Unknown             // Unknown relationship (Enum version mismatch?)
  };

  /**
   * EnumToString - Lifted from the P2996 proposal, since that really
   * is about the easiest way to do that. Everyone's going to write one
   * of these. Feel free to use your own if it's better. This won't
   * exist in compiled code if you don't use it.
   */

  template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
  constexpr std::string_view EnumToString(E value) {
    if constexpr (Enumerable) {
      template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (value == [: e :]) {
          return std::meta::identifier_of(e);
        }
      }
    }
    // Differentiate from defined Unknown, but <Unknown> will covert back to
    // Unknown in StringToEnum
    return "<Unknown>";    
  }

  /**
   * And the reverse. You can use this generically but if you pass it a Relationship
   * and the string isn't found, it'll return Relationship::Unknown rather than nullopt.
   */
  
  template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
  constexpr std::optional<E> StringToEnum(std::string_view name) {
    if constexpr (Enumerable) {
      template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (name == std::meta::identifier_of(e)) {
          return [: e :];
        }
      }

      if constexpr (std::is_same_v<E, Relationship>) {
        return Relationship::Unknown;
      }
    }
    return std::nullopt;
  }

}
