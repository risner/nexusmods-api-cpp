cmake_minimum_required(VERSION 3.14)
project(nexusmods LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

add_library(nexusmods STATIC
    src/client.cpp
)

target_include_directories(nexusmods
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include	# for this library
        ${DEPS_DIR}/cpp-httplib		# httplib single-header dir
        ${DEPS_DIR}/rapidjson/include	# rapidjson include dir
)

target_compile_definitions(nexusmods PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
find_package(OpenSSL REQUIRED)
target_include_directories(nexusmods PUBLIC ${OPENSSL_INCLUDE_DIR})
target_link_libraries(nexusmods PUBLIC OpenSSL::SSL OpenSSL::Crypto pthread)

add_executable(example_app examples/example_main.cpp)
target_link_libraries(example_app PRIVATE nexusmods)
