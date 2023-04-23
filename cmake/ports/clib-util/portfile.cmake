# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO powerof3/CLibUtil
    REF bf52162882f92d0ceccccd3a75305ed31b3efafa
    SHA512 37ced7ee80b63663a7ddaf0c50d74ec32b7459b0e4e3f3f6db3afbd69faebe727ccad7a97ad48bf02686f345a5fbf7da960ff9ea0f24a52afeceb8078b608ad7
    HEAD_REF master
)

# Install codes
set(CLIBUTIL_SOURCE	${SOURCE_PATH}/include/ClibUtil)
file(INSTALL ${CLIBUTIL_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
