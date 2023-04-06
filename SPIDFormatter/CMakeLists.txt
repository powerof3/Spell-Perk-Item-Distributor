add_project(
	TARGET_TYPE EXECUTABLE
	PROJECT SPIDFormatter
	VERSION 1.1.0
	INCLUDE_DIRECTORIES
		src
		${CLIB_UTIL_INCLUDE_DIRS}
		${SRELL_INCLUDE_DIRS}
	GROUPED_FILES
		"src/main.cpp"
)

find_path(CLIB_UTIL_INCLUDE_DIRS "ClibUtil/utils.hpp")
find_path(SRELL_INCLUDE_DIRS "srell.hpp")

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
		cxx_std_23
)
