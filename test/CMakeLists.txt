include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.0
)
FetchContent_MakeAvailable(Catch2)

include(Catch)

add_executable(libgp_parser_tests
    test_gpx_archive.cpp
    test_gpx_format.cpp
    test_gpx_score_reader.cpp
    test_gpx_track_reader.cpp
    test_gtp_reader.cpp
    test_gpx_document.cpp
    test_load_song.cpp
    test_domain_model.cpp
    test_song.cpp
    test_tempo.cpp
    test_timeline.cpp
    test_track.cpp
)

target_compile_definitions(libgp_parser_tests
    PRIVATE
        LIBGP_PARSER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

target_link_libraries(libgp_parser_tests
    PRIVATE
        libgp_parser
        miniz
        Catch2::Catch2WithMain
)

catch_discover_tests(libgp_parser_tests)
