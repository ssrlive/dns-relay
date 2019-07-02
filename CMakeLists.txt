# CMakeList.txt : CMake project for DNS Relay, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++17 -Wno-narrowing -Wno-c++98-compat -Wno-reserved-id-macro -Wno-nonportable-system-include-path -Wno-sign-conversion")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Zi")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2")
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("C:/Project/libuv/include")
INCLUDE_DIRECTORIES("C:/Project/uvw")
AUX_SOURCE_DIRECTORY(src DIR_SRCS)
add_executable(dnsRelay
   ${DIR_SRCS}
)
if(WIN32)
target_link_libraries(dnsRelay ws2_32 libuv IPHLPAPI Psapi Userenv libuv) 
elseif(UNIX)
target_link_libraries(dnsRelay uv)
endif()

# TODO: Add tests and install targets if needed.
