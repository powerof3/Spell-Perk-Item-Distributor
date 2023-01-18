vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO martinus/unordered_dense
    REF 612f2b5b2a4d44bb74a40debf6c3ff86ab476c62
    SHA512 fe23fba77f9501dc386bb03a79b06471603b8d5729c6dcb3ff5abf6d7406fa5a5cc103fd95538d743bbdfd764400036d6c9ac4b70168ed2f98e9407c54d3ce2e
    HEAD_REF main
)

vcpkg_cmake_configure(
	SOURCE_PATH "${SOURCE_PATH}"
	OPTIONS
		-DRH_STANDALONE_PROJECT=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
	PACKAGE_NAME unordered_dense
	CONFIG_PATH lib/cmake/unordered_dense
)

file(REMOVE_RECURSE
	"${CURRENT_PACKAGES_DIR}/debug"
	"${CURRENT_PACKAGES_DIR}/lib"
)

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
