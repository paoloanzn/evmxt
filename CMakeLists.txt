cmake_minimum_required(VERSION 3.10)

project(evmxt LANGUAGES C)

option(EVMXT_BUILD_C_BINDINGS_ONLY "Build only the Lua C bindings" OFF)

if(EVMXT_BUILD_C_BINDINGS_ONLY)
    add_subdirectory(lua-lib/c-bindings)
    return()
endif()


add_executable(evmxt kernel/main.c
    kernel/abi.c
    kernel/lua_abi.c
    kernel/rpc.c
    kernel/hex.c
    kernel/eip_1559_tx.c
    kernel/rlp.c
    kernel/eth_crypto.c
    kernel/selector.c
    kernel/runtime.c
    kernel/operations.c)
target_include_directories(evmxt PRIVATE kernel)

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

option(EVMXT_BUILD_TESTS "Build Lua ABI conversion and send_transaction tests" OFF)
if(EVMXT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
