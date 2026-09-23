cmake_minimum_required(VERSION 4.0)

project(test_macro_expander LANGUAGES C CXX)

if (MSVC)
add_compile_options("/source-charset:utf-8")
endif()

add_executable(test_macro_expander
main.cpp
)

