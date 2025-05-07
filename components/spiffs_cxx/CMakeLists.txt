set(
    sources
        "src/Spiffs.cpp"
)

idf_component_register(
    SRCS
        ${sources}
    INCLUDE_DIRS
        include
    REQUIRES
        spiffs
        vfs
)
