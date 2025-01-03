cmake_minimum_required(VERSION 3.16)

project(TclEasyBeast
  VERSION 0.1
  DESCRIPTION "Tcl Http extension"
  LANGUAGES C)

add_library(TclEasyBeast SHARED
    src/lib.c
    )

set_target_properties(TclEasyBeast PROPERTIES
    OUTPUT_NAME "easy_beast")

add_compile_options(-Wall -fsanitize=address,undefined)
target_compile_options(TclEasyBeast PRIVATE -v -lstdc++ -Wno-incompatible-pointer-types)

 # CMake knows how to find TCL
find_package(TCL)
find_package(TclStub)
add_compile_definitions(USE_TCL_STUBS)

#target_link_libraries(TclEasyBeast -lEasyBeast)

find_path(TCL_INCLUDE_DIR tcl.h REQUIRED
    HINTS
    ${CMAKE_SOURCE_DIR}/../tcl/generic
    ${CMAKE_SOURCE_DIR}/../tcl86/generic
    ${CMAKE_SOURCE_DIR}/../tcl87/generic
    /usr/local/tcl86/include /usr/local/tcl87/include /usr/include/tcl8.6)

target_include_directories(TclEasyBeast
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${TCL_INCLUDE_PATH}"
  "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(TclEasyBeast
    PRIVATE
    ${TCL_STUB_LIBRARY}
    stdc++
    EasyBeast)

# Copy TCL package index
add_custom_target(copy_pkg_index SOURCES
    "easy_beast/pkgIndex.tcl"
    COMMAND
    ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/easy_beast
        ${CMAKE_CURRENT_BINARY_DIR}/easy_beast)

# Copy test folder to binary directory
add_custom_target(copy_tcl_tests SOURCES
    "tests/all.tcl"
    "tests/server.test"
    COMMAND
    ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_BINARY_DIR}/tests)

# Copy examples folder to binary directory
add_custom_target(copy_examples SOURCES
    "examples/http_server.tcl"
    COMMAND
    ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${CMAKE_CURRENT_BINARY_DIR}/examples)

add_dependencies(TclEasyBeast copy_pkg_index copy_tcl_tests copy_examples)

# copy dll into http_parser package directory
add_custom_command(TARGET TclEasyBeast POST_BUILD
    COMMAND
    ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:TclEasyBeast>
        ${CMAKE_CURRENT_BINARY_DIR}/easy_beast)

add_custom_target(
  EasyBeastTest ALL
  COMMAND cd tests && tclsh all.tcl
  COMMENT "run TCL tests")
