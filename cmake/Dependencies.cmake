include(FetchContent)

set(LIBGP_PARSER_SYSTEM_DEPS TRUE)

# --- pugixml ---
find_package(pugixml CONFIG QUIET)
if(pugixml_FOUND)
    message(STATUS "Using system pugixml")
else()
    set(LIBGP_PARSER_SYSTEM_DEPS FALSE)
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

# --- miniz (ZIP for GPX/GP7 containers) ---
find_package(miniz CONFIG QUIET)
if(miniz_FOUND)
    message(STATUS "Using system miniz")
    if(TARGET miniz::miniz AND NOT TARGET miniz)
        add_library(miniz ALIAS miniz::miniz)
    endif()
else()
    set(LIBGP_PARSER_SYSTEM_DEPS FALSE)
    message(STATUS "miniz not found — FetchContent 3.1.2")
    FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG 3.1.2
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(miniz)
endif()

if(TARGET miniz::miniz)
    set(LIBGP_PARSER_MINIZ_TARGET miniz::miniz)
elseif(TARGET miniz)
    set(LIBGP_PARSER_MINIZ_TARGET miniz)
else()
    message(FATAL_ERROR "miniz target not found after dependency resolution")
endif()
