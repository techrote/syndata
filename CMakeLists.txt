cmake_minimum_required(VERSION 3.24)

project(SynData LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SynData requires a 64-bit build.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(SYNDATA_WARNINGS_AS_ERRORS "Treat SynData compiler warnings as errors" ON)

function(syndata_apply_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
            /fp:precise
            $<$<BOOL:${SYNDATA_WARNINGS_AS_ERRORS}>:/WX>
        )
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            $<$<BOOL:${SYNDATA_WARNINGS_AS_ERRORS}>:-Werror>
        )
    endif()
endfunction()

add_library(syndata_engine STATIC
    src/engine/execution.cpp
    src/engine/graph.cpp
    src/engine/hash.cpp
    src/engine/prng.cpp
    src/engine/recipe.cpp
)
target_include_directories(syndata_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
syndata_apply_warnings(syndata_engine)

add_executable(SynData src/app/main.cpp)
target_link_libraries(SynData PRIVATE syndata_engine)
syndata_apply_warnings(SynData)

add_executable(SynDataEngineTests tests/engine_test.cpp)
target_link_libraries(SynDataEngineTests PRIVATE syndata_engine)
syndata_apply_warnings(SynDataEngineTests)

add_executable(SynDataRecipeTests tests/recipe_execution_test.cpp)
target_link_libraries(SynDataRecipeTests PRIVATE syndata_engine)
syndata_apply_warnings(SynDataRecipeTests)

enable_testing()
add_test(NAME syndata.engine COMMAND SynDataEngineTests)
add_test(NAME syndata.recipe-execution COMMAND SynDataRecipeTests)
add_test(NAME syndata.version COMMAND SynData --version)
set_tests_properties(syndata.version PROPERTIES PASS_REGULAR_EXPRESSION "SynData SD-002-dev")
add_test(NAME syndata.help COMMAND SynData --help)
set_tests_properties(syndata.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: SynData")
add_test(NAME syndata.self-check COMMAND SynData --self-check)
set_tests_properties(syndata.self-check PROPERTIES PASS_REGULAR_EXPRESSION "self-check OK")
add_test(NAME syndata.recipe.validate COMMAND SynData recipe validate "${CMAKE_CURRENT_SOURCE_DIR}/examples/sd002-minimal.sdr")
set_tests_properties(syndata.recipe.validate PROPERTIES PASS_REGULAR_EXPRESSION "valid-sdr")
add_test(NAME syndata.recipe.inspect COMMAND SynData recipe inspect "${CMAKE_CURRENT_SOURCE_DIR}/examples/sd002-minimal.sdr")
set_tests_properties(syndata.recipe.inspect PROPERTIES PASS_REGULAR_EXPRESSION "SynData recipe")
add_test(NAME syndata.recipe.canonicalize COMMAND SynData recipe canonicalize "${CMAKE_CURRENT_SOURCE_DIR}/examples/sd002-minimal.sdr")
set_tests_properties(syndata.recipe.canonicalize PROPERTIES PASS_REGULAR_EXPRESSION "sdr 1")
add_test(NAME syndata.recipe.fingerprint COMMAND SynData recipe fingerprint "${CMAKE_CURRENT_SOURCE_DIR}/examples/sd002-minimal.sdr")
set_tests_properties(syndata.recipe.fingerprint PROPERTIES PASS_REGULAR_EXPRESSION "[0-9a-f][0-9a-f][0-9a-f][0-9a-f]")
