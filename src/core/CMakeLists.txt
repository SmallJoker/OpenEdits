file(GLOB my_files *.cpp)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/script")

set(CORE_FILES
	${SCRIPT_FILES}
	${my_files}
	"${CMAKE_CURRENT_SOURCE_DIR}/sha3.c"
	PARENT_SCOPE
)
