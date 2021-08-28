cmake_minimum_required(VERSION 3.2)

project(kczp)

set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_C_FLAGS "-Wpedantic")
#set(CMAKE_C_FLAGS "-fsanitize=address,undefined")

file(GLOB image "src/types.c" 
	"src/iofile.c" "src/image.c")

add_executable(enc "src/enc.c")
add_executable(dec "src/dec.c")

add_executable(edges "src/edges_detection.c")

target_sources(enc PUBLIC ${image})
target_sources(dec PUBLIC ${image})
target_sources(edges PUBLIC ${image})



target_link_libraries(dec SDL2 SDL2_image zstd m)
target_link_libraries(enc SDL2 SDL2_image zstd m)
target_link_libraries(edges SDL2 SDL2_image zstd m)
