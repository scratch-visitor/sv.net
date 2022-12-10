cmake_minimum_required(VERSION 3.0)

set(MOD_NAME sv.net.tester)
set(SOURCE_FILES test.cpp)
set(HEADER_FILES )

add_executable(${MOD_NAME} ${SOURCE_FILES})
target_link_libraries(${MOD_NAME} "sv.net")