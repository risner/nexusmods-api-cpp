# nexusmods-api-cpp
C++ Library for Nexus Mod API


# Include in CMakeLists.txt
```
set(NEXUSMODS_DIR ${CMAKE_SOURCE_DIR}/deps/nexusmods-api-cpp)
add_subdirectory(${NEXUSMODS_DIR})
target_include_directories(yourmod PUBLIC
	${NEXUSMODS_DIR}/include
	${NEXUSMODS_DIR}/deps/cpp-httplib
	${NEXUSMODS_DIR}/deps/rapidjson/include
)
target_link_libraries(yourmod PRIVATE
	nexusmods
)
```
# Premium required to get download links
get_file_download_link()