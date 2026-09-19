cmake_minimum_required(VERSION 3.10)

project(evmxt LANGUAGES C)


add_executable(evmxt main.c
    rpc.c
    hex.c
    eip_1559_tx.c
    rlp.c
    eth_crypto.c
    selector.c
    runtime.c)

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
