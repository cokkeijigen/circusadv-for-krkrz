cmake_minimum_required(VERSION 3.20.0)
project(circusadv_for_krkrz C CXX)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	add_compile_options("/source-charset:utf-8")
endif()

add_subdirectory(utils)
add_subdirectory(test)