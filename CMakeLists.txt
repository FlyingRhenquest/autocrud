cmake_minimum_required(VERSION 4.2)

project(AutoCrud LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOCRUD_BUILD_MODULES "Build autocrud as a C++ module" OFF)
option(AUTOCRUD_BUILD_TESTS "Build autocrud tests" ON)

# These are potentially dangerous to your database -- don't enable them unless you really need to
option(AUTOCRUD_BUILD_INTEGRATION_TESTS "Build integration tests (WILL WRITE TO YOUR DATABASE)" OFF)

# Warn about integraiton tests
if (AUTOCRUD_BUILD_INTEGRATION_TESTS)
  message(WARNING "Integration tests are being built. The integration tests will write to your database and drop records and tables for integration test objects. You should only run these on a carefully isolated database instance expressly reserved for such purposes. I'll TRY to keep them safe...ish... but don't go running them against a production database instance or even an integration database that other testers are working on. I'd go so far as to say that these should probably only ever be run in an isolated docker VM or something like that.")
  # don't say I didn't warn you!
endif()

find_package(PkgConfig)
find_package(Boost 1.90 REQUIRED)
find_package(libpqxx CONFIG REQUIRED)

set(LIBRARY_LIST)

list(APPEND LIBRARY_LIST Boost::boost)
list(APPEND LIBRARY_LIST libpqxx::pqxx)

if (AUTOCRUD_BUILD_MODULES)
  add_library(autocrud)
  add_library(FR::autocrud ALIAS autocrud)
  
  target_sources(autocrud
    PUBLIC
      FILE_SET CXX_MODULES FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/fr/autocrud/fr.autocrud.cppm
  )
else()
  add_library(autocrud INTERFACE)
  add_library(FR::autocrud ALIAS autocrud)
endif()

target_include_directories(autocrud PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(autocrud PUBLIC cxx_std_26)

target_link_libraries(autocrud PUBLIC
  ${LIBRARY_LIST}
)

if (AUTOCRUD_BUILD_TESTS)
  add_subdirectory(test)
endif()
