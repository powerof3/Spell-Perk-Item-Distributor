# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO powerof3/CLibUtil
    REF ee0c71260aa25a6e4686aad1ce2d55bde2e5a808
    SHA512 beff7dacb50dc8963bd65db0528c9e74b542e24f3e6218e77e5b43ab134a9cc531c7f81fd67c699072c53af39ddf9b8cd7d6467ae74305a2e75a4905a25d14d0
    HEAD_REF master
)

# Install codes
set(CLIBUTIL_SOURCE	${SOURCE_PATH}/include/ClibUtil)
file(INSTALL ${CLIBUTIL_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
