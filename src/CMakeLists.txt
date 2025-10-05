set(APP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")

idf_component_register(
    SRCS
        "main.c"
        "ui.c"
        "waveforms.c"
    INCLUDE_DIRS
        "."
        "${APP_INCLUDE_DIR}"
    REQUIRES
        ssd1306
        driver
)
