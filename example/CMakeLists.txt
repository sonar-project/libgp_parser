add_executable(gp_info main.cpp)
target_link_libraries(gp_info PRIVATE libgp_parser)

add_executable(gp_score gp_score.cpp)
target_link_libraries(gp_score PRIVATE libgp_parser)
