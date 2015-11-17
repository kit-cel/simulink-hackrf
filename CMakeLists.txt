cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

##########################################################################
project(simulink_hackrf LANGUAGES C VERSION 1.0.0)
include("local.cmake" OPTIONAL)  # defs to make IDE happy

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/Documents/MATLAB"
        CACHE PATH "default install prefix" FORCE)
endif()
set(INSTALL_DESTINATION "." CACHE PATH "install directory relative to prefix")


##########################################################################
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(LIBHACKRF REQUIRED)
get_filename_component(HACKRF_LIB_DIR ${LIBHACKRF_LIBRARIES} DIRECTORY)
find_package(Matlab REQUIRED)
if(NOT MATLAB_MEX_PATH)
    find_program(MATLAB_MEX_PATH mex mex.bat
        HINTS ${MATLAB_ROOT}/bin
        PATHS ${MATLAB_ROOT}/bin
        DOC "The mex program path"
    )
    if(NOT MATLAB_MEX_PATH)
        message(FATAL_ERROR "Can't find Matlab MEX compiler")
    endif()
endif()

##########################################################################
set(mex_extra_args "-silent" "-largeArrayDims")
if(CMAKE_VERBOSE_MAKEFILE)
    list(APPEND mex_extra_args "-v")
endif()
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    list(APPEND mex_extra_args "-g")
endif()


##########################################################################
add_subdirectory(src)
add_subdirectory(blockset)


##########################################################################
# uninstall target
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)
add_custom_target(
    uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)