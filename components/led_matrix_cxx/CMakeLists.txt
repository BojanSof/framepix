set(
    sources
        "src/led_strip_encoder.c"
        "src/LedMatrix.cpp"
        "src/MatrixAnimator.cpp"
)

idf_component_register(
    SRCS
        ${sources}
    INCLUDE_DIRS
        include
    REQUIRES
        esp_driver_rmt
)
