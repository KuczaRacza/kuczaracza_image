cmake_minimum_required(VERSION 3.2)

project(kczp)

set(CMAKE_C_FLAGS "-Wall")

file(GLOB image "src/types.c" 
	"src/iofile.c" "src/image.c")
add_executable(enc "src/enc.c" ${image})
add_executable(dec "src/dec.c" ${image})
target_link_libraries(dec SDL2 SDL2_image m)
target_link_libraries(enc SDL2 SDL2_image m)