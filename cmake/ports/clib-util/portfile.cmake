# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO powerof3/CLibUtil
    REF bccb84d54fbb96af1aa3e393df07677eabacf391
    SHA512 1d2ec54529751356f2d71096b7fa3268d06f0edfc0579c6225356717f7a8d77da5a467b2baf2090403b1a93e9d8bbf396b8df43e5e2619d76a7cd08f96c06eba
    HEAD_REF master
)

# Install codes
set(CLIBUTIL_SOURCE	${SOURCE_PATH}/include/ClibUtil)
file(INSTALL ${CLIBUTIL_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
