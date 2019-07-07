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
if(WIN32)
INCLUDE_DIRECTORIES("C:/Project/libuv/include")
INCLUDE_DIRECTORIES("C:/Project/uvw")
AUX_SOURCE_DIRECTORY(src DIR_SRCS)
set(CPP_REDIS_FOLDER_PATH "C:/Project/cpp_redis")
INCLUDE_DIRECTORIES("${CPP_REDIS_FOLDER_PATH}/includes")
INCLUDE_DIRECTORIES("${CPP_REDIS_FOLDER_PATH}/tacopie/includes")
endif()
add_executable(dnsRelay
   ${DIR_SRCS}
)
if(WIN32)
target_link_libraries(dnsRelay ws2_32 IPHLPAPI Psapi Userenv libuv cpp_redis tacopie) 
elseif(UNIX)
target_link_libraries(dnsRelay uv)
endif()

# TODO: Add tests and install targets if needed.
