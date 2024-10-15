cmake_minimum_required(VERSION 3.29)
project(udp_raw_socket)

set(CMAKE_CXX_STANDARD 14)

add_executable(udp_raw_socket dnsRawSocket.cpp)
