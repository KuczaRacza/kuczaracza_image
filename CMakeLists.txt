cmake_minimum_required(VERSION 3.2)

project(kczp)

set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-Wpedantic")
set(CMAKE_CXX_FLAGS "-fsanitize=address,undefined")
set(CMAKE_CXX_STANDARD 17)
file(GLOB image "src/types.c" 
	"src/iofile.c" "src/image.c")

add_executable(enc "src/enc.c")
add_executable(dec "src/dec.c")

target_sources(enc PUBLIC ${image})
target_sources(dec PUBLIC ${image})

target_link_libraries(dec SDL2 SDL2_image m)
target_link_libraries(enc SDL2 SDL2_image m)