# Set up project
cmake_minimum_required(VERSION 3.16)
project(OpenEdits)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/" ${CMAKE_MODULE_PATH})

message("--- Setting up OpenEdits")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type: Release or Debug" FORCE)
endif()


### Libraries

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED) # pthread
find_package(ENet REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(ZLIB)

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

if (MSVC)
	# Need .pdb
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO")

	# Skip unnecessary headers
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi /D WIN32_LEAN_AND_MEAN /D NOCOMM")
	set(CMAKE_CXX_FLAGS_RELEASE "/Ox /MD /EHa /D NDEBUG")
	set(CMAKE_CXX_FLAGS_DEBUG "/Od /MDd /RTC1")

else()
	# -fsanitize=address,undefined
	# -O3 is default for release
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -gdwarf-4")

	# Debugging symbols
	set(CMAKE_CXX_FLAGS_DEBUG "-g -O1")

	set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
endif()

### Inline version information

add_custom_target(SetVersion
	COMMAND ${CMAKE_COMMAND}
	-D "PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
	-D "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
	-P "${PROJECT_SOURCE_DIR}/cmake/SetVersion.cmake"
)


### Source files

add_subdirectory("${PROJECT_SOURCE_DIR}/src")

get_target_property(IRRLICHT_INCLUDES IrrlichtMt::IrrlichtMt INTERFACE_INCLUDE_DIRECTORIES)
include_directories(
	"${PROJECT_SOURCE_DIR}/src"
	${ENet_INCLUDE_DIRS}
	${IRRLICHT_INCLUDES}
	${SQLite3_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIRS}
)

if(MSVC)
	file(TO_CMAKE_PATH ${SQLite3_INCLUDE_DIR} SQLite3_INCLUDE_DIR)
	# idk. I cannot get this to link properly otherwise
	set(SRC_FILES
		${SRC_FILES}
		${SQLite3_INCLUDE_DIR}/sqlite3.c
	)
endif()

### Executable

file(WRITE "${PROJECT_SOURCE_DIR}/src/version.cpp" "#error file needs update")
add_executable(${PROJECT_NAME} ${SRC_FILES})
add_dependencies(${PROJECT_NAME} SetVersion)


### Libraries

target_link_libraries(
	${PROJECT_NAME}
	${ENet_LIBRARY}
	Threads::Threads
	IrrlichtMt::IrrlichtMt
)

if(ZLIB_FOUND)
	message("--- Found ZLIB for EEO world import")
	add_compile_definitions(HAVE_ZLIB)
	include_directories(${ZLIB_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} ZLIB::ZLIB)
endif()

if(MSVC)
	# For ENet
	target_link_libraries(${PROJECT_NAME} ws2_32.lib winmm.lib)
else()
	target_link_libraries(${PROJECT_NAME} SQLite::SQLite3)
endif()
