cmake_minimum_required(VERSION 3.12)

project(evmxt_lua_bindings LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(unofficial-lua CONFIG REQUIRED)
find_package(libsecp256k1 CONFIG REQUIRED)
find_package(OpenSSL 3.2 REQUIRED)

file(GLOB BINDING_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
add_library(bindings MODULE
    ${BINDING_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../selector.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../eth_crypto.c"
)
target_include_directories(bindings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(bindings PRIVATE
    unofficial::lua::lua
    libsecp256k1::secp256k1
    OpenSSL::Crypto
)

# Lua loads the module beside its source, with the same suffix on both platforms.
set_target_properties(bindings PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(APPLE)
    target_link_libraries(bindings PRIVATE "-framework Security" "-framework CoreFoundation")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SECRET REQUIRED IMPORTED_TARGET libsecret-1)
    target_link_libraries(bindings PRIVATE PkgConfig::SECRET)
else()
    message(FATAL_ERROR "Lua C bindings support only macOS and Linux")
endif()
