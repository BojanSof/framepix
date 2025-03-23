set(
    sources
        "src/HttpServer.cpp"
)

idf_component_register(
    SRCS
        ${sources}
    INCLUDE_DIRS
        include
    REQUIRES
        esp_http_server
)
