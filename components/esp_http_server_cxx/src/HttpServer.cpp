#include "HttpServer.hpp"

namespace EspHttpServer
{
esp_err_t HttpServer::start(uint16_t port)
{
    config_ = HTTPD_DEFAULT_CONFIG();
    config_.server_port = port;
    config_.stack_size = 10240;
    return httpd_start(&server_, &config_);
}

esp_err_t HttpServer::stop() { return httpd_stop(server_); }

esp_err_t HttpServer::registerUri(HttpUri& uri)
{
    return httpd_register_uri_handler(
        getNativeHandle(), &uri.getNativeHandle());
}

esp_err_t HttpServer::unregisterUri(HttpUri& uri)
{
    return httpd_unregister_uri(getNativeHandle(), uri.getNativeHandle().uri);
}
}  // namespace EspHttpServer
