add_project(
	TARGET_TYPE EXECUTABLE
	PROJECT SPIDFormatter
	VERSION 1.0.0
	INCLUDE_DIRECTORIES
		src
	GROUPED_FILES
		"src/main.cpp"
)

find_package(Boost MODULE REQUIRED COMPONENTS regex)
find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		Boost::regex
)
