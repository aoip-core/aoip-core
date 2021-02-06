cmake_minimum_required(VERSION 3.1)
project(aoip C)

set(CMAKE_C_STANDARD 11)

IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
ENDIF()

set(CMAKE_C_FLAGS" ${CMAKE_C_FLAGS} -Wall -Wextra -O")


find_package(Threads REQUIRED)

# libptpc
add_library(ptpc STATIC
		lib/ptpc.c
        lib/socket.c)
target_include_directories(ptpc PUBLIC include)
target_link_libraries(ptpc)

# libsap
add_library(sap STATIC
		lib/sap.c
        lib/socket.c)
target_include_directories(sap PUBLIC include)
target_link_libraries(sap)

# librtp
add_library(rtp STATIC
		lib/rtp.c
        lib/socket.c)
target_include_directories(rtp PUBLIC include)
target_link_libraries(rtp)

# libaoip
add_library(aoip STATIC
		lib/aoip.c
		lib/wav.c
		lib/pcap.c
		lib/tone.c
		)
target_include_directories(aoip PUBLIC include)
target_link_libraries(aoip sap ptpc rtp)


# simple-ptpc
add_executable(simple-ptpc src/simple-ptpc/simple-ptpc.c)
target_link_libraries(simple-ptpc ptpc)

# simple-sapc
add_executable(simple-sapc src/simple-sapc/simple-sapc.c)
target_link_libraries(simple-sapc sap)

# simple-rtpc-recv
add_executable(simple-rtpc-recv src/simple-rtpc/simple-rtpc-recv.c)
target_link_libraries(simple-rtpc-recv rtp)

# tonegen
add_executable(tonegen src/tonegen/tonegen.c)
target_link_libraries(tonegen Threads::Threads m aoip)

# rtpdump
add_executable(rtpdump src/rtpdump/rtpdump.c)
target_link_libraries(rtpdump Threads::Threads aoip)


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

