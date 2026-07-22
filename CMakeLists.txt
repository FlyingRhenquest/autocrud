cmake_minimum_required(VERSION 4.2)

project(AutoCrud LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOCRUD_BUILD_TESTS "Build autocrud tests" OFF)

# These are potentially dangerous to your database -- don't enable them unless you really need to
option(AUTOCRUD_BUILD_INTEGRATION_TESTS "Build integration tests (WILL WRITE TO YOUR DATABASE)" OFF)

# Build the Vector Embeddings tests. This requires you to have a gguf file in resources/tiny.gguf.
# You can download one from huggingface suitable for unit testing with hf:
# mkdir resources
# pip install -U hf
# hf download tensorblock/optimum-internal-testing_tiny-random-qwen3-GGUF --local-dir ./resources
#
# This model is random and just used for unit testing. We'll need a different one for
# actual applications.
OPTION(AUTOCRUD_BUILD_VECTOR_EMBEDDING_TESTS OFF)

# Warn about integraiton tests
if (AUTOCRUD_BUILD_INTEGRATION_TESTS)
  message(WARNING "Integration tests are being built. The integration tests will write to your database and drop records and tables for integration test objects. You should only run these on a carefully isolated database instance expressly reserved for such purposes. I'll TRY to keep them safe...ish... but don't go running them against a production database instance or even an integration database that other testers are working on. I'd go so far as to say that these should probably only ever be run in an isolated docker VM or something like that.")
  # don't say I didn't warn you!
endif()

find_package(PkgConfig)
find_package(Boost 1.90 REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(llama CONFIG REQUIRED)

set(INTERFACE_HEADERS
  "include/fr/autocrud/Crud.h"
  "include/fr/autocrud/CrudTypes.h"
  "include/fr/autocrud/EmbeddingEngine.h"
  "include/fr/autocrud/Graph.h"
  "include/fr/autocrud/Helpers.h"
  "include/fr/autocrud/Index.h"
  "include/fr/autocrud/Node.h"
  "include/fr/autocrud/Query.h"
  "include/fr/autocrud/RelationTypes.h"
  "include/fr/autocrud/VectorType.h"
)


set(LIBRARY_LIST)

list(APPEND LIBRARY_LIST Boost::boost)
list(APPEND LIBRARY_LIST libpqxx::pqxx)

add_library(autocrud INTERFACE)

set_target_properties(autocrud PROPERTIES
  FRAMEWORK TRUE
  PUBLIC_HEADER "${INTERFACE_HEADERS}"
)

add_library(FR::autocrud ALIAS autocrud)

target_include_directories(autocrud INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(autocrud INTERFACE cxx_std_26)

target_link_libraries(autocrud INTERFACE
  ${LIBRARY_LIST}
)

if (AUTOCRUD_BUILD_TESTS)
  add_subdirectory(test)
endif()

set(FRautocrud_VERSION_MAJOR 0)
set(FRautocrud_VERSION_MINOR 1)
set(FRautocrud_VERSION ${FRautocrud_VERSION_MAJOR}.${FRautocrud_VERSION_MINOR})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/FRautocrudConfigVersion.cmake
  VERSION "${FRautocrud_VERSION}"
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/FRautocrudConfig.cmake
  INSTALL_DESTINATION lib/cmake/FRautocrud
)

install(TARGETS autocrud
  EXPORT autocrud_exports
  DESTINATION include/fr/autocrud
)

install(EXPORT autocrud_exports
  FILE autocrudTargets.cmake
  NAMESPACE FRautocrud::
  DESTINATION lib/cmake/FRautocrud
)

install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/include/fr/autocrud.h"
  DESTINATION
  include/fr
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/FRautocrudConfigVersion.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/FRautocrudConfig.cmake"
  DESTINATION lib/cmake/FRautocrud
)
