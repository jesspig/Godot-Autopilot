cmake_minimum_required(VERSION 3.20)
project(gda_ipc_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ipc_spike_server server.cpp)

if(WIN32)
  target_link_libraries(ipc_spike_server PRIVATE ws2_32)
endif()
