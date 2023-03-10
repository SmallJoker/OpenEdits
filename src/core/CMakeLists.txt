file(GLOB my_files *.cpp)


set(CORE_FILES
	${my_files}
	"${CMAKE_CURRENT_SOURCE_DIR}/sha3.c"
	PARENT_SCOPE
)
