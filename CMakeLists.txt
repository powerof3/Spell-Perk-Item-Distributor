cmake_minimum_required(VERSION 3.20)
include("cmake/common.cmake")

project(SPID-common)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

function(conditionally_add_subdirectory SRC_DIR)
	if(NOT TARGET "${SUBDIRECTORY}")
		if("${ARGC}" GREATER 1)
			add_subdirectory("${SRC_DIR}" "${ARGV1}")
		else()
			add_subdirectory("${SRC_DIR}")
		endif()
	endif()
endfunction()

conditionally_add_subdirectory(SPID)
conditionally_add_subdirectory(SPIDFormatter)
