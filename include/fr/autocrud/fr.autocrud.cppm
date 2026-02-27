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

module;

#include <fr/autocrud/Crud.h>
#include <fr/autocrud/CrudTypes.h>
#include <fr/autocrud/Helpers.h>
#include <fr/autocrud/Node.h>

export module fr.autocrud;

export namespace fr::autocrud {
    using fr::autocrud::DbIgnore;
    using fr::autocrud::DbFieldType;
    using fr::autocrud::DbFieldName;
    using fr::autocrud::Crud;
    using fr::autocrud::TypeToDbType;
    using fr::autocrud::DbFormatData;
    using fr::autocrud::FixedString;
    using fr::autocrud::Node;

    using fr::autocrud::operator""_ColumnType;
    using fr::autocrud::operator""_ColumnName;
}
