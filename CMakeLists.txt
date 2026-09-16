cmake_minimum_required(VERSION 3.24)

project(SynData LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SynData SD-001 requires a 64-bit build.")
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

# Domain-neutral deterministic substrate transplanted from the ArtMiner AM-016
# seam. There is intentionally no platform UI, renderer, domain evaluator,
# built-in node catalog, dataset layer, or third-party runtime dependency here.
add_library(syndata_engine STATIC
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

enable_testing()
add_test(NAME syndata.engine COMMAND SynDataEngineTests)
add_test(NAME syndata.version COMMAND SynData --version)
set_tests_properties(syndata.version PROPERTIES PASS_REGULAR_EXPRESSION "SynData SD-001-dev")
add_test(NAME syndata.help COMMAND SynData --help)
set_tests_properties(syndata.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: SynData")
add_test(NAME syndata.self-check COMMAND SynData --self-check)
set_tests_properties(syndata.self-check PROPERTIES PASS_REGULAR_EXPRESSION "self-check OK")
