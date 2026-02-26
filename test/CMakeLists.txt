cmake_minimum_required(VERSION 4.2)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)
include(GoogleTest)
enable_testing()

set(TEST_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/NodeTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/NodeCrud.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/AutocrudTests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/HelpersTest.cpp
)

add_executable(test
  ${TEST_SRC}
)

target_link_libraries(test PUBLIC
  GTest::GTest
  GTest::Main
  FR::autocrud
)

if (BUILD_INTEGRATION_TESTS)

  set(INTEGRATION_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/IntegrationTests.cpp
  )
  add_executable(IntegrationTests
    ${INTEGRATION_TEST_SRC}
  )

  target_link_libraries(IntegrationTests PUBLIC
    GTest::GTest
    GTest::Main
    FR::autocrud
  )
  
endif()
