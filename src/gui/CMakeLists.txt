file(GLOB current_files *.cpp)
file(GLOB gameplay_files gameplay/*.cpp)
file(GLOB guilayout_files guilayout/*.cpp)


set(GUI_FILES
	${current_files}
	${gameplay_files}
	${guilayout_files}
	PARENT_SCOPE
)
