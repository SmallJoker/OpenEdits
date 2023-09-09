file(GLOB current_files *.cpp)
file(GLOB gameplay_files gameplay/*.cpp)


set(GUI_FILES
	${current_files}
	${gameplay_files}
	PARENT_SCOPE
)
