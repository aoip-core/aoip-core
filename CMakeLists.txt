cmake_minimum_required(VERSION 3.1)
project(aoip C)

set(CMAKE_C_STANDARD 11)

IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
ENDIF()

set(CMAKE_C_FLAGS" ${CMAKE_C_FLAGS} -Wall -Wextra -O")


find_package(Threads REQUIRED)

# libaoip
add_library(aoip STATIC
		lib/aoip.c
		lib/sap.c
		lib/wav.c
		lib/pcap.c
		lib/tone.c
)
target_include_directories(aoip PUBLIC include)
target_link_libraries(aoip)

# libptpc
add_library(ptpc STATIC
		lib/ptpc.c
		lib/socket.c)
target_include_directories(ptpc PUBLIC include)
target_link_libraries(ptpc)

# simple-ptpc
add_executable(simple-ptpc src/simple-ptpc/simple-ptpc.c)
target_link_libraries(simple-ptpc ptpc)

# capture
add_executable(capture
	src/capture/capture.c
	src/capture/myapp.h
	src/capture/signal.c
	src/capture/network.c
	src/capture/audio.c
)
target_link_libraries(capture Threads::Threads aoip)


# test
add_executable(test_sap test/test_sap.c picotest/picotest.c)
target_include_directories(test_sap PRIVATE picotest)
target_link_libraries(test_sap aoip)

enable_testing()
add_test(test test_sap)

IF(CMAKE_BUILD_TYPE MATCHES Release)
add_custom_command(
	TARGET test_sap
	COMMENT "Run tests"
	POST_BUILD
	COMMAND test_sap
)
ENDIF()

