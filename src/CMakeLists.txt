# pass build off to MATLAB mex script
macro(add_hackrf_mex_library name args)
    add_custom_command(
        OUTPUT ${name}.${Matlab_MEX_EXTENSION}
        COMMAND ${MATLAB_MEX_PATH}
        ARGS ${mex_extra_args} ${args}
             -I${CMAKE_CURRENT_SOURCE_DIR}
             -I${LIBHACKRF_INCLUDE_DIR}
             -L${HACKRF_LIB_DIR}
             -lhackrf
             -outdir ${CMAKE_BINARY_DIR}
             ${CMAKE_CURRENT_SOURCE_DIR}/${name}.c
             ${CMAKE_CURRENT_SOURCE_DIR}/common.c
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${name}.c
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/common.c
    )
    add_custom_target(${name} ALL DEPENDS ${name}.${Matlab_MEX_EXTENSION})
    install(FILES ${CMAKE_BINARY_DIR}/${name}.${Matlab_MEX_EXTENSION}
            DESTINATION ${INSTALL_DESTINATION})
endmacro()

add_hackrf_mex_library(hackrf_source "")

add_hackrf_mex_library(hackrf_sink "")

add_hackrf_mex_library(hackrf_find_devices "-DSIMULINK_HACKRF_VERSION=${PROJECT_VERSION}")

add_custom_command(
    TARGET hackrf_find_devices POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/hackrf_find_devices.m ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_BINARY_DIR}/hackrf_find_devices.m DESTINATION ${INSTALL_DESTINATION})
