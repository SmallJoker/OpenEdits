# Set up project
cmake_minimum_required(VERSION 3.13)
project(OpenEdits)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/" ${CMAKE_MODULE_PATH})

message("--- Setting up OpenEdits")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type: Release or Debug" FORCE)
endif()

set(BUILD_CLIENT TRUE CACHE BOOL "Whether to include the client part")


### Libraries

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED) # pthread
find_package(ENet REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)

set(IRRLICHTMT_BUILD_DIR "" CACHE PATH "Client: Path to IrrlichtMt build directory. Server: Include path")
if (BUILD_CLIENT)
	message("--- Type: Client (GUI) & Server")

	set(OPENAL_FOUND FALSE)
	find_package(OpenAL)
	message("--- OpenAL sound support: ${OPENAL_FOUND}")

	# Irrlicht detection code imported from minetest/CMakeLists.cpp, commit 38ba813c by JosiahWI
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
else()
	message("--- Type: Server only")
	project(OpenEdits-server)

	# The server only needs headers, so give it headers.
	find_path(IRRLICHT_INCLUDE_DIR NAMES irrlicht.h
		PATHS
		"${IRRLICHTMT_BUILD_DIR}"
		"${IRRLICHTMT_BUILD_DIR}/include"
		"${IRRLICHTMT_BUILD_DIR}/include/irrlichtmt"
	)
	if (NOT IRRLICHT_INCLUDE_DIR)
		unset(IRRLICHT_INCLUDE_DIR CACHE)
		message(FATAL_ERROR "Cannot find irrlicht.h inside IRRLICHT_INCLUDE_DIR")
	endif()

	add_library(IrrlichtMt::IrrlichtMt INTERFACE IMPORTED)
	set_target_properties(IrrlichtMt::IrrlichtMt PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${IRRLICHT_INCLUDE_DIR}")
	unset(IRRLICHT_INCLUDE_DIR CACHE)
endif()


# Scripting support (optional for now)

find_package(LuaJIT)
set(HAVE_LUA FALSE)

if(LUAJIT_FOUND)
	message("--- Found LuaJIT")
	set(HAVE_LUA TRUE)
else()
	find_package(Lua51) # -> LUA51_FOUND
	if(LUA51_FOUND)
		message("--- Found Lua: ${LUA_VERSION_STRING}")
		set(HAVE_LUA TRUE)
	endif()
endif()

if(NOT HAVE_LUA)
	message(FATAL_ERROR "Lua not found but is required.")
endif()


### Compiler options

# Is there no easier way?
if (BUILD_CLIENT)
	add_compile_definitions(BUILD_CLIENT=1)
else()
	add_compile_definitions(BUILD_CLIENT=0)
endif()

if (MSVC)
	# Need .pdb
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO")

	# Skip unnecessary headers
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi /MP /D WIN32_LEAN_AND_MEAN /D NOCOMM")
	set(CMAKE_CXX_FLAGS_RELEASE "/Ox /MD /EHa /D NDEBUG")
	set(CMAKE_CXX_FLAGS_DEBUG "/Od /MDd /RTC1")
else()
	# -fsanitize=address,undefined
	# -O3 is default for release
	# -g instead of -gdwarf-4 saves about 20% of the binary size
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

	# Debugging symbols
	set(CMAKE_CXX_FLAGS_DEBUG "-g -O1")

	set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
endif()

if (MINGW)
	add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
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

if (MSVC)
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
	ZLIB::ZLIB
)

if (MINGW)
	# Needed by ENet but is not carried over (?)
	target_link_libraries(${PROJECT_NAME} winmm ws2_32)
endif()

if(BUILD_CLIENT)
	target_link_libraries(${PROJECT_NAME} IrrlichtMt::IrrlichtMt)
endif()

if(OPENAL_FOUND)
	add_compile_definitions(HAVE_SOUND)
	include_directories(${OPENAL_INCLUDE_DIR})
	target_link_libraries(${PROJECT_NAME} ${OPENAL_LIBRARY})

	set (OPENAL_OLDER_THAN_1_21_0 FALSE CACHE BOOL "Forwards declaration changed with 1.21.0.")
	if (OPENAL_OLDER_THAN_1_21_0)
		add_compile_definitions(OPENAL_OLDER_THAN_1_21_0)
	endif()
endif()

if(HAVE_LUA)
	include_directories(${LUA_INCLUDE_DIR})
	target_link_libraries(${PROJECT_NAME}
		${LUA_LIBRARIES} # FindLua51
		${LUA_LIBRARY}   # FindLuaJIT
	)
endif()

if(MSVC)
	# For ENet
	target_link_libraries(${PROJECT_NAME} ws2_32.lib winmm.lib)
elseif(HAIKU)
	target_link_libraries(${PROJECT_NAME} SQLite::SQLite3)
else()
	target_link_libraries(${PROJECT_NAME} stdc++fs SQLite::SQLite3)
endif()


### Installation

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/build")
set(INSTALL_DIR ${PROJECT_NAME})


# General files

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${INSTALL_DIR}
		LIBRARY DESTINATION ${INSTALL_DIR}
		ARCHIVE DESTINATION ${INSTALL_DIR}
		BUNDLE DESTINATION .
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION ${INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" DESTINATION ${INSTALL_DIR})
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/misc/licenses" DESTINATION ${INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt" DESTINATION "${INSTALL_DIR}/licenses" RENAME "openedits.txt")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/worlds/README.txt" DESTINATION "${INSTALL_DIR}/worlds")

if(WIN32)
	# Copy DLL directly
	install(FILES ${ENet_LIBRARY} DESTINATION ${INSTALL_DIR})
	install(FILES ${ZLIB_LIBRARY} DESTINATION ${INSTALL_DIR})
	install(FILES ${LUA_LIBRARY}  DESTINATION ${INSTALL_DIR})
	if(OPENAL_FOUND)
		install(FILES ${OPENAL_LIBRARY} DESTINATION ${INSTALL_DIR})
	endif()
else()
	# Resolve symlink and perform hack
	get_filename_component(ENET_DLL ${ENet_LIBRARY} REALPATH) # resolve symlink
	get_filename_component(ENET_DLL_NAME ${ENet_LIBRARY} NAME) # compile time filename
	install(FILES ${ENET_DLL} DESTINATION ${INSTALL_DIR} RENAME "${ENET_DLL_NAME}.7") # hack
endif()

if(BUILD_CLIENT)
	get_target_property(
		IRRLICHT_DLL IrrlichtMt::IrrlichtMt IMPORTED_LOCATION_RELEASE)
	install(FILES ${IRRLICHT_DLL} DESTINATION ${INSTALL_DIR})
endif()


# Platform-specific installs

if(LINUX OR UNIX)
	# Start script
	install(FILES "${PROJECT_SOURCE_DIR}/misc/AppRun.sh"
		DESTINATION ${INSTALL_DIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
