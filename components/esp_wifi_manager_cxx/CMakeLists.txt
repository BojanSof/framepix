set(
    sources
        "src/WifiManager.cpp"
)

idf_component_register(
    SRCS
        ${sources}
    INCLUDE_DIRS
        include
    REQUIRES
        esp_wifi
        esp_event
)
