cmake_minimum_required(VERSION 3.1)
project(aoip C)

set(CMAKE_C_STANDARD 11)

IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
ENDIF()

set(CMAKE_C_FLAGS" ${CMAKE_C_FLAGS} -Wall -Wextra -O")


find_package(Threads REQUIRED)

# libptpc
add_library(ptpc STATIC lib/ptpc.c lib/socket.c)
target_include_directories(ptpc PUBLIC include)
target_link_libraries(ptpc)

# libsap
add_library(sap STATIC lib/sap.c lib/socket.c)
target_include_directories(sap PUBLIC include)
target_link_libraries(sap)

# librtp
add_library(rtp STATIC lib/rtp.c lib/socket.c)
target_include_directories(rtp PUBLIC include)
target_link_libraries(rtp)

# libaoip
add_library(aoip STATIC
	lib/aoip.c
	lib/pcap.c
	lib/ptpc.c
	lib/rtp.c
	lib/sap.c
	lib/socket.c
	lib/tone.c
	lib/wav.c)
target_include_directories(aoip PUBLIC include)
target_link_libraries(aoip m)
install(TARGETS aoip DESTINATION /usr/local/lib)
install(FILES
	include/aoip.h
	DESTINATION /usr/local/include)
install(FILES
	include/aoip/pcap.h
	include/aoip/ptp.h
	include/aoip/ptpc.h
	include/aoip/queue.h
	include/aoip/rtp.h
	include/aoip/sap.h
	include/aoip/socket.h
	include/aoip/timer.h
	include/aoip/tone.h
	include/aoip/utils.h
	include/aoip/wav.h
	DESTINATION /usr/local/include/aoip)

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
target_link_libraries(rtpdump Threads::Threads m aoip)


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

