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

# Irrlicht detection code imported from minetest/CMakeLists.cpp, commit 38ba813c by JosiahWI
set(IRRLICHTMT_BUILD_DIR "" CACHE PATH "Path to IrrlichtMt build directory.")
find_package(IrrlichtMt QUIET
	PATHS "${IRRLICHTMT_BUILD_DIR}"
	NO_DEFAULT_PATH
)

if(NOT TARGET IrrlichtMt::IrrlichtMt)
	# find_package() searches certain subdirectories. ${PATH}/cmake is not
	# the only one, but it is the one where IrrlichtMt is supposed to export
	# IrrlichtMtConfig.cmake
	message(FATAL_ERROR "Could not find IrrlichtMtConfig.cmake in ${IRRLICHTMT_BUILD_DIR}/cmake.")
endif()


### Compiler options

if (NOT MSVC)
	# -fsanitize=address,undefined
	# -O3 is default for release
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

	# Debugging symbols
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O1")

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()


### Source files

add_subdirectory("${PROJECT_SOURCE_DIR}/src")

get_target_property(IRRLICHT_INCLUDES IrrlichtMt::IrrlichtMt INTERFACE_INCLUDE_DIRECTORIES)
include_directories(
	"${PROJECT_SOURCE_DIR}/src"
	${ENet_INCLUDE_DIRS}
	${IRRLICHT_INCLUDES}
)


### Executable

add_executable(${PROJECT_NAME} ${SRC_FILES})


target_link_libraries(
	${PROJECT_NAME}
	${ENet_LIBRARY}
	Threads::Threads
	IrrlichtMt::IrrlichtMt
)
