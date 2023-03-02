
execute_process(
	COMMAND git describe --always --abbrev=7 --dirty
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	OUTPUT_VARIABLE GIT_SHA1
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file("${PROJECT_SOURCE_DIR}/src/version.cpp.in" "${PROJECT_SOURCE_DIR}/src/version.cpp")
