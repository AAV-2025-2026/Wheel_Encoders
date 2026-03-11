set(TARGET_NAME wheel_encoder)

add_executable(${TARGET_NAME}
    ${TARGET_NAME}.cpp
    )

target_link_libraries(${TARGET_NAME} PRIVATE
    pico_stdlib
    ETHERNET_FILES
    IOLIBRARY_FILES
    )

pico_add_extra_outputs(${TARGET_NAME})
