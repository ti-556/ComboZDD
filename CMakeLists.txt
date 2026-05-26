cmake_minimum_required(VERSION 3.20)
project(combo_lens_v1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(combolens INTERFACE)
target_include_directories(combolens INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(combo_lens_demo src/main.cpp)
target_link_libraries(combo_lens_demo PRIVATE combolens)

if (MSVC)
  target_compile_options(combo_lens_demo PRIVATE /W4)
else()
  target_compile_options(combo_lens_demo PRIVATE -Wall -Wextra -pedantic)
endif()

add_executable(combo_lens_tests tests/test_basic.cpp)
target_link_libraries(combo_lens_tests PRIVATE combolens)

if (MSVC)
  target_compile_options(combo_lens_tests PRIVATE /W4)
else()
  target_compile_options(combo_lens_tests PRIVATE -Wall -Wextra -pedantic)
endif()

enable_testing()
add_test(NAME combo_lens_basic_tests COMMAND combo_lens_tests)
