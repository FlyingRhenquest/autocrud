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

#include <fr/autocrud/CrudTypes.h>
#include <format>
#include <string>
#include <meta>
#include <ranges>
#include <vector>

namespace fr::autocrud {

  /**
   * A structure to hold query text
   */
  struct QueryText {    
    char const* Text;
  };

  /**
   * Query enables an arbitrary SQL query to be run and populated into
   * a vector of a data type designed to receive the data in the query.
   *
   * The data type in the Query parameter must have an QueryText annotation
   * with the SQL to run in the .Text field. The field names of the struct
   * should match the column names being retrieved from the database.
   * Since the query isn't associated with a specific table, the Query datatype
   * doesn't need to derive from Node. The query will just be run and the fields
   * in the structure will just be populated by whatever the database returns.
   */

  template <typename Receiver>
  struct Query {

  private:
    // Access context for std::meta
    constexpr static auto _ctx = std::meta::access_context::unchecked();
    // Max members in storage
    constexpr static size_t _maxMembers = std::meta::nonstatic_data_members_of(^^Receiver, _ctx).size();

    // Retrieve SQL query from user-supplied annotation    
    static consteval auto GetSqlQuery() {
      static_assert(std::meta::annotations_of_with_type(^^Receiver, ^^QueryText).size() == 1, "Receiver class must have one and only one QueryText annotation.");
      return std::meta::extract<QueryText>(std::meta::annotations_of_with_type(^^Receiver, ^^QueryText)[0]).Text;
    }

    /**
     * Define what our column tuple looks like.
     */
    template <typename MemberPtrType>
    using ColumnTuple = std::tuple<
      const char*, // Field Name
      MemberPtrType>;

    template <std::meta::info field>
    static constexpr auto MakeColumnTuple() {
      using Ptr = decltype(&[: field :]);
      return ColumnTuple<Ptr> {
        std::define_static_string(std::meta::identifier_of(field)),
        &[: field :]
      };
    }

    // Lay out our query's column definitions
    template <size_t memberIndex, typename... Accumulator>
    static constexpr auto BuildDef(Accumulator... blob) {
      constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^Receiver, _ctx));

      if constexpr (memberIndex == fields.size()) {
        return std::tuple<Accumulator...>{blob...};
      } else {
        return BuildDef<memberIndex + 1>(blob..., MakeColumnTuple<fields[memberIndex]>());
      }
    }

    // Entrypoint into BuildDef
    static constexpr auto BuildDefs() {
      return BuildDef<0>();
    }
    
    // Store reference to storage passed in constructor
    std::vector<Receiver>& _storage;

    static constexpr auto _columns = BuildDefs();
  public:

    static constexpr auto columnsSize = std::tuple_size<decltype(_columns)>{};
    static constexpr auto lastColumn = std::tuple_size<decltype(_columns)>{} - 1;
    static constexpr auto sql = GetSqlQuery();
        
    Query(std::vector<Receiver>& storage) : _storage(storage) {}

    /**
     * Return column tuple by index. You can convert it into
     * structured bindings with
     * auto [fieldname, ptr] = column<0>();
     */
    template <size_t index>
    constexpr auto& column() {
      return std::get<index>(_columns);
    }

    /**
     * Run the query. You must pass in a pqxx::params that matches the
     * number of parameters in your query SQL and a pqxx::conection.
     * Returns true if any records were returned or false if none
     * were. THe records will be populated into the storage you passed
     * in the constructor.
     */
    bool run(pqxx::params &p, pqxx::connection &c) {
      bool ret = false;
      pqxx::work work(c);
      pqxx::result res = work.exec(sql, p);
      if (res.size() > 0) {
        ret = true;
      }
      // Just iterate through our columns setting the values of each column from
      // the database row.
      for (auto const &row : res) {
        Receiver recv;
        template for(constexpr size_t i : std::views::iota(0, columnsSize)) {
          const auto [fieldname, ptr] = this->column<i>();
          using ColumnType = std::decay_t<decltype(recv.*ptr)>;
          using ReadType = DbFormatData<ColumnType>::ReadType;
          DbFormatData<ColumnType>::set(recv.*ptr, row[std::string(fieldname)].template as<ReadType>());
        }
        _storage.push_back(recv);
      }
      return ret;
    }
  };
}
