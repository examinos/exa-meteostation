# List any additional sources here
target_sources(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
    application.c
    angle_average.c
    exa_reed_switch.c
    exa_flip_flop.c
    )

# If you added some folder with header files you need to list them here
target_include_directories(
    ${CMAKE_PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
