
file(GLOB SERVER_FILES server/*.cpp)
file(GLOB TESTS_FILES tests/*.cpp)

# Directories for API bindings
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/core")

set(SRC_FILES
	${CORE_FILES}
	${SERVER_FILES}
	${TESTS_FILES}
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/version.cpp
)

if (BUILD_CLIENT)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/client")
	file(GLOB GUI_FILES gui/*.cpp)

	list(APPEND SRC_FILES
		${CLIENT_FILES}
		${GUI_FILES}
	)
endif()

# And now propagate the list to the parent directory...
set(SRC_FILES
	${SRC_FILES}
	PARENT_SCOPE
)
