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

/**
 * Some non-namespaced helpers for shorter annotations
 */

#include <algorithm>
#include <fr/autocrud/Crud.h>
#include <meta>
#include <string_view>

// Ignore maps to fr::autocrud::DbIgnore

consteval auto Ignore() {
  return fr::autocrud::DbIgnore{};
}

// TODO: Replace with std::basic_fixed_string if P3094R0 ever
// gets implemented

namespace fr::autocrud {

  template <std::size_t N>
  struct FixedString {
    char data[N];
    constexpr FixedString(const char (&str)[N]) {
      std::copy_n(str, N, data);
    }
  };
  
}

/**
 * Map DbFieldType to _ColumnType maybe?
 */

template <fr::autocrud::FixedString T>
constexpr fr::autocrud::DbFieldType operator""_ColumnType() {
  return fr::autocrud::DbFieldType(std::define_static_string(T.data));
}

/**
 * And DbFieldName to _ColumnName
 */

template <fr::autocrud::FixedString T>
constexpr fr::autocrud::DbFieldName operator""_ColumnName() {
  return fr::autocrud::DbFieldName(std::define_static_string(T.data));
}

template <fr::autocrud::FixedString T>
constexpr fr::autocrud::DbTableName operator""_TableName() {
  return fr::autocrud::DbTableName(std::define_static_string(T.data));
}
