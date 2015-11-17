add_custom_target(blockset ALL DEPENDS hackrf_library.slx slblocks.m)

add_custom_command(
    TARGET blockset POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/hackrf_library.slx ${CMAKE_BINARY_DIR}
)
add_custom_command(
    TARGET blockset POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/slblocks.m ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_BINARY_DIR}/slblocks.m ${CMAKE_BINARY_DIR}/hackrf_library.slx
        DESTINATION ${INSTALL_DESTINATION})
