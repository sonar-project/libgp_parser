cmake_minimum_required(VERSION 3.20)

project(
    libgp_parser
    VERSION 0.2.1
    DESCRIPTION "Guitar Pro file parser (GP,GPX/GP5) — standalone C++ library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands" FORCE)

option(LIBGP_PARSER_BUILD_TESTS "Build unit tests" ON)
option(LIBGP_PARSER_BUILD_EXAMPLE "Build example CLI" OFF)

include(cmake/Dependencies.cmake)

# Default to static when embedded (FetchContent); Gentoo/cmake.eclass sets
# BUILD_SHARED_LIBS=ON for a proper shared install.
add_library(libgp_parser
    src/binary_reader.cpp include/libgp_parser/binary_reader.hpp
    src/gpx_archive.cpp include/libgp_parser/gpx_archive.hpp
    src/gpx_format.cpp  include/libgp_parser/gpx_format.hpp
    src/gpx_gp6_filesystem.cpp include/libgp_parser/gpx_gp6_filesystem.hpp
    src/gpx_score_bundle.cpp include/libgp_parser/gpx_score_bundle.hpp
    src/gpx_score_reader.cpp include/libgp_parser/gpx_score_reader.hpp
    src/gpx_tempo_reader.cpp include/libgp_parser/gpx_tempo_reader.hpp
    src/gpx_track_reader.cpp include/libgp_parser/gpx_track_reader.hpp
    src/gpx_document_reader.cpp include/libgp_parser/gpx_document.hpp
    src/gpx_document_parser.cpp include/libgp_parser/gpx_document.hpp
    src/gtp_reader.cpp include/libgp_parser/gtp_reader.hpp
    src/load_song.cpp include/libgp_parser/load_song.hpp
    src/tempo.cpp include/libgp_parser/tempo.hpp
    src/timeline.cpp include/libgp_parser/timeline.hpp
    src/tuning_pitches.cpp include/libgp_parser/tuning_pitches.hpp
)

add_library(libgp_parser::libgp_parser ALIAS libgp_parser)

set_target_properties(libgp_parser PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    OUTPUT_NAME gp_parser
)

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
        ${LIBGP_PARSER_MINIZ_TARGET}
)

if(LIBGP_PARSER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(LIBGP_PARSER_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

# Package export only when deps are imported system packages. FetchContent
# builds (embedded by SonarPractice) must not install nested miniz/pugixml.
if(LIBGP_PARSER_SYSTEM_DEPS)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS libgp_parser
        EXPORT libgp_parserTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT libgp_parserTargets
        FILE libgp_parserTargets.cmake
        NAMESPACE libgp_parser::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libgp_parser
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/libgp_parserConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
    )
    configure_package_config_file(
        cmake/libgp_parserConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/libgp_parserConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libgp_parser
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/libgp_parserConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/libgp_parserConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libgp_parser
    )
else()
    message(STATUS "Skipping libgp_parser install rules (bundled deps via FetchContent)")
endif()
