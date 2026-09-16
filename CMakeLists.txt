cmake_minimum_required(VERSION 3.10)

project(evmxt LANGUAGES C)


add_executable(evmxt main.c)

find_package(unofficial-lua CONFIG REQUIRED)
target_link_libraries(evmxt PRIVATE unofficial::lua::lua)
