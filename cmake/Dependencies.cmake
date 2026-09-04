include(FetchContent)

set(LIBGP_PARSER_SYSTEM_PUGIXML FALSE)

# --- OpenSSL (PRIVATE — GP7/GP8 edit-locked score.gpif decryption) ---
find_package(OpenSSL REQUIRED)

# --- pugixml (PUBLIC — exposed via include/libgp_parser/gpx_xml.hpp) ---
find_package(pugixml CONFIG QUIET)
if(pugixml_FOUND)
    set(LIBGP_PARSER_SYSTEM_PUGIXML TRUE)
    message(STATUS "Using system pugixml")
else()
    message(STATUS "pugixml not found — FetchContent v1.16")
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.16
    )
    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(pugixml)
endif()

# --- miniz (PRIVATE — ZIP for GPX/GP7; not packaged on Ubuntu, link statically) ---
find_package(miniz CONFIG QUIET)
if(miniz_FOUND)
    message(STATUS "Using system miniz")
    if(TARGET miniz::miniz AND NOT TARGET miniz)
        add_library(miniz ALIAS miniz::miniz)
    endif()
else()
    message(STATUS "miniz not found — FetchContent 3.1.2 (static, PIC)")
    # Keep caller's BUILD_SHARED_LIBS (e.g. ON for packaging) for libgp_parser itself.
    set(_libgp_parser_saved_build_shared "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG 3.1.2
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(miniz)
    if(DEFINED _libgp_parser_saved_build_shared)
        set(BUILD_SHARED_LIBS "${_libgp_parser_saved_build_shared}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
endif()

if(TARGET miniz::miniz)
    set(LIBGP_PARSER_MINIZ_TARGET miniz::miniz)
elseif(TARGET miniz)
    set(LIBGP_PARSER_MINIZ_TARGET miniz)
else()
    message(FATAL_ERROR "miniz target not found after dependency resolution")
endif()

# Static FetchContent miniz must be PIC when linked into a shared libgp_parser.
if(TARGET miniz AND NOT miniz_FOUND)
    set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
