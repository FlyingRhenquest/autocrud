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
#include <string>
#include <string_view>
#include <fr/autocrud/VectorType.h>

namespace fr::autocrud {

  // TODO: Replace with std::basic_fixed_string if P3094R0 ever
  // gets implemented

  template <std::size_t N>
  struct FixedString {
    char data[N];
    constexpr FixedString(const char (&str)[N]) {
      std::copy_n(str, N, data);
    }
    constexpr FixedString(const std::array<char,N>& str) {
      std::copy_n(str.data(), N, data);
    }
  };
  
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
   * Store a fr::autocrud::Vector as a vector
   */

  template <size_t parameters>
  struct TypeToDbType<::fr::autocrud::Vector<parameters>> {
    
    static consteval size_t countDigits(size_t value) {
      if (value == 0) {
        return 1;
      }
      size_t count = 0;
      while(value > 0) {
        count++;
        value /= 10;
      }
      return count;
    }

    template <size_t N>
    static constexpr auto toString() {
      constexpr size_t digits = countDigits(N);
      std::array<char, digits+1> out{};
      size_t temp = N;
      for(size_t i = 0; i < digits; ++i) {
        out[digits - 1 - i] = static_cast<char>('0' + (temp % 10));
        temp /= 10;
      }
      out[digits] = '\0';
      return out;
    }

    template <size_t dimensions>
    struct Builder {
      static constexpr char const* prefix = std::define_static_string("public.vector(");
      static constexpr char const* suffix = std::define_static_string(")");
      static constexpr auto numStr = toString<dimensions>();
      // 8 = length of "vector(" + length of ")". numStr has a null in it that we're counting too.
      static constexpr size_t total_size = 15 + numStr.size();

      static constexpr std::array<char, total_size> build() {
        std::array<char, total_size> data;
        size_t idx = 0;
        const char *p = prefix;
        while(*p) {
          data[idx++] = *p++;
        }
        for (size_t i = 0; i < numStr.size() - 1; ++i) {
          data[idx++] = numStr[i];
        }
        p = suffix;
        while(*p) {
          data[idx++] = *p++;
        }
        data[idx] = '\0';
        return data;
      }
      
      static constexpr FixedString storage{build()};      

    };

    static constexpr char const* fieldType = Builder<parameters>::storage.data;
    
  };


  /**
   * DbFormatData formats data for a type. Most of them won't do anything to the
   * type. Chrono needs some special handling
   */

  template <typename T>
  struct DbFormatData {
    /**
     * ReadType will be the C++ type used to retrieve the type from
     * the Postgres row with .as<Type>.
     */
    using ReadType = T;

    inline static const T& format(const T& value) {
      return value;
    }

    /**
     * Set the value in the struct to the value from the database
     * easy version (just sets the thing to the thing)
     */
    inline static void set(T& element, const ReadType& value) {
      element = value;
    }
    
  };

  template <>
  struct DbFormatData<std::chrono::system_clock::time_point> {

    // Apparently pqxx knows how to do this
    using ReadType = std::chrono::system_clock::time_point;
    
    inline static std::string format(const std::chrono::system_clock::time_point &value) {
      return std::format(":{:%FT%TZ}", value);
    }

    // So I can use the simple version of set
    inline static void set(ReadType &element, const ReadType& value) {
      element = value;
    }
  };

  // Vectors take some more work
  
  template <size_t parameters>
  struct DbFormatData<::fr::autocrud::Vector<parameters>> {

    using ReadType = std::string;
    
    inline static std::string format(const fr::autocrud::Vector<parameters> &value) {
      return value.toPgString();
    }

    inline static void set(fr::autocrud::Vector<parameters>& element, const std::string& value) {
      element.parseEmbeddings(value);
    }
  };
}
