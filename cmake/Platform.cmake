# Platform / Architecture / CI detection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(GSD_ARCH "x86_64")
else()
    set(GSD_ARCH "x86")
endif()

if(DEFINED ENV{CI})
    set(GSD_IS_CI ON)
    message(STATUS "[gsd] CI environment detected")
else()
    set(GSD_IS_CI OFF)
endif()

message(STATUS "[gsd] Platform: ${CMAKE_SYSTEM_NAME} ${GSD_ARCH}")
message(STATUS "[gsd] Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
