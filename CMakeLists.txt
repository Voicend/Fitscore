cmake_minimum_required(VERSION 3.10)
project(FITSCore)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(include)
include_directories(include/nanomsg)

set(LIB_SUBDIR "")
set(PLATFORM_UTILS 3rd/utils2.c)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
	list(APPEND PLATFORM_UTILS "3rd/unix/platform_utils.c")
	set(LIB_SUBDIR /macos)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	# using GCC
	list(APPEND PLATFORM_UTILS "3rd/unix/platform_utils.c")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
	list(APPEND PLATFORM_UTILS "3rd/win32/platform_utils.c")
endif()

link_directories(lib${LIB_SUBDIR})
message("link_directories:" lib${LIB_SUBDIR})

add_executable(fits-core
        assigner.hpp
        common.hpp
        configuration.cpp
        configuration.hpp
        envaluator.cpp
        envaluator.hpp
        fits_core.cpp
        jobs.cpp
        jobs.hpp
        loader.cpp
        loader.hpp
        machine.cpp
        machine.hpp
        models.hpp
        productline.hpp
        shiftbook.hpp
        simulator.cpp
        simulator.hpp
        taskinfo.hpp
        tasks.cpp
        tasks.hpp
        utils.cpp
        utils.hpp
        voter.cpp
        voter.hpp
	${PLATFORM_UTILS})

target_link_libraries(fits-core jsoncpp rabbitmq pthread)
