# Set up project
cmake_minimum_required(VERSION 3.16)
project(OpenEdits)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/" ${CMAKE_MODULE_PATH})

message("--- Setting up OpenEdits")


### Libraries

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED) # pthread
find_package(ENet REQUIRED)


### Compiler options

if (NOT MSVC)
	# -O3 is default for release
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

	# Debugging symbols
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()


### Source files

add_subdirectory("${PROJECT_SOURCE_DIR}/src")

include_directories(
	"${PROJECT_SOURCE_DIR}/src"
	${ENet_INCLUDE_DIRS}
)

### Executable

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(
	${PROJECT_NAME}
	${ENet_LIBRARY}
)
