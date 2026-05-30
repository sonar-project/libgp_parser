cmake_minimum_required(VERSION 3.20)

project(
    libgp_parser
    VERSION 0.1.0
    DESCRIPTION "Guitar Pro file parser (GP,GPX/GP5) — standalone C++ library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands" FORCE)

option(LIBGP_PARSER_BUILD_TESTS "Build unit tests" ON)
option(LIBGP_PARSER_BUILD_EXAMPLE "Build example CLI" ON)

include(cmake/Dependencies.cmake)

# enable cppcheck
find_program(CPPCHECK_PATH cppcheck)
if(CPPCHECK_PATH)
    set(CMAKE_CXX_CPPCHECK
        ${CPPCHECK_PATH}
        --enable=warning,performance,portability
        --inline-suppr
        --suppress=missingIncludeSystem
        --std=c++23
        --suppress=*:*/_deps/*
    )
else()
    message(WARNING "Cppcheck not found!")
endif()

add_library(libgp_parser STATIC
    src/binary_reader.cpp 
    src/gpx_archive.cpp
    src/gpx_format.cpp
    src/gpx_gp6_filesystem.cpp
    src/gpx_score_bundle.cpp
    src/gpx_score_reader.cpp
    src/gpx_tempo_reader.cpp
    src/gpx_track_reader.cpp
    src/gtp_reader.cpp
    src/load_song.cpp
    src/tempo.cpp
    src/tuning_pitches.cpp
)

add_library(libgp_parser::libgp_parser ALIAS libgp_parser)

target_include_directories(libgp_parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(libgp_parser PUBLIC cxx_std_23)

target_link_libraries(libgp_parser
    PUBLIC
        pugixml::pugixml
    PRIVATE
        miniz
)

if(LIBGP_PARSER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(LIBGP_PARSER_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
