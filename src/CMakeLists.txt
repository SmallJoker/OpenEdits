
file(GLOB GUI_FILES gui/*.cpp)
file(GLOB SERVER_FILES server/*.cpp)
file(GLOB TESTS_FILES tests/*.cpp)

# Directories for API bindings
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/client")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/core")

set(SRC_FILES
	${CLIENT_FILES}
	${CORE_FILES}
	${GUI_FILES}
	${SERVER_FILES}
	${TESTS_FILES}
	${UTILS_FILES}
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	PARENT_SCOPE
)
