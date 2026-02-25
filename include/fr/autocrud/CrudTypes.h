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
#include <chrono>
#include <format>
#include <time.h>
#include <meta>

namespace fr::autocrud {

  /**
   * Handle type conversions between C++ and Database
   *
   * The nonspecialized type will probably not work often, so
   * you should specialize the ones you plan to use frequently
   * or use the DbFieldType annotation to specify the types
   * you want.
   */

  template <typename T>
  struct TypeToDbType {
    static constexpr char* fieldType = std::meta::identifier_of(^^T).data();
  };

  template <>
  struct TypeToDbType<std::string> {
    static constexpr char const* fieldType = std::define_static_string("TEXT");
  };

  /**
   * Ints on most Linux architectures these days are 64 bits, so
   * you need to store them in a bigint.
   */
  
  template <>
  struct TypeToDbType<int> {
    static constexpr char const* fieldType = std::define_static_string("BIGINT");
  };

  template <>
  struct TypeToDbType<long> {
    static constexpr char const* fieldType = std::define_static_string("BIGINT");
  };

  template <>
  struct TypeToDbType<float> {
    static constexpr char const* fieldType = std::define_static_string("REAL");
  };

  template <>
  struct TypeToDbType<double> {
    static constexpr char const* fieldType = std::define_static_string("FLOAT8");
  };

  /**
   * Store a std::chrono::system_clock::timestamp as a timestamp
   */

  template <>
  struct TypeToDbType<std::chrono::system_clock::time_point> {
    static constexpr char const* fieldType = std::define_static_string("TIMESTAMP");
  };


  /**
   * DbFormatData formats data for a type. Most of them won't do anything to the
   * type. Chrono needs some special handling
   */

  template <typename T>
  struct DbFormatData {

    inline static const T& format(const T& value) {
      return value;
    }
    
  };

  template <>
  struct DbFormatData<std::chrono::system_clock::time_point> {
    inline static std::string format(const std::chrono::system_clock::time_point &value) {
      return std::format(":{:%FT%TZ}", value);
    }
  };
}
