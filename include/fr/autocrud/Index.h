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

#include <array>
#include <fr/autocrud/CrudTypes.h>
#include <string>

namespace fr::autocrud {

  /**
   * An index object. This tells the Crud object to create an index.
   * You attach it to the struct with an annotation.
   *
   * Crud will just jam a ; on the end of the string it creates if
   * there isn't one there when it gets done constructing the comamnd.
   */

  struct Index {
    // Index name
    const char *Name = nullptr;
    // If On is left null, the Crud object will simply use the name
    // of the field the index is attached to. If you want to do anything
    // fancy with the index, you'll want to set this.  The Crud object
    // will emit "ON " so you don't need to include that part.
    const char *On = nullptr;
    // If using is left null, it won't be used. If it is set, the Crud object
    // will append "USING " your using value to the create index command.
    const char *Using = nullptr;
    // If Where is defined, it will be applied after ON. If both Where and Using
    // are defined, Using will be applied last.
    const char *Where = nullptr;
  };

  /**
   * Contains some number of indexes
   */
  
  template <size_t IndexCount>
  struct IndexArray {
    std::array<Index,IndexCount> index;
  };
}
