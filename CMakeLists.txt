cmake_minimum_required(VERSION 3.10)

project(evmxt LANGUAGES C)

option(EVMXT_BUILD_C_BINDINGS_ONLY "Build only the Lua C bindings" OFF)

if(EVMXT_BUILD_C_BINDINGS_ONLY)
    add_subdirectory(lua-lib/c-bindings)
    return()
endif()


add_executable(evmxt main.c
    rpc.c
    hex.c
    eip_1559_tx.c
    rlp.c
    eth_crypto.c
    selector.c
    runtime.c
    operations.c)

find_package(unofficial-lua CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(libsecp256k1 CONFIG REQUIRED)
find_package(cJSON CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(evmxt PRIVATE 
    unofficial::lua::lua
    CURL::libcurl
    libsecp256k1::secp256k1 
    cjson
    OpenSSL::Crypto
)
