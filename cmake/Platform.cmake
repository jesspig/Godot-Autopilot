# Platform / Architecture / CI detection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(GDA_ARCH "x86_64")
else()
    set(GDA_ARCH "x86")
endif()

if(DEFINED ENV{CI})
    set(GDA_IS_CI ON)
    message(STATUS "[gda] CI environment detected")
else()
    set(GDA_IS_CI OFF)
endif()

message(STATUS "[gda] Platform: ${CMAKE_SYSTEM_NAME} ${GDA_ARCH}")
message(STATUS "[gda] Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
