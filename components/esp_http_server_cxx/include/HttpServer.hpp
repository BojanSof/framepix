#ifndef _ESP_HTTP_SERVER_CXX_HTTP_SERVER_HPP
#define _ESP_HTTP_SERVER_CXX_HTTP_SERVER_HPP

#include "HttpUri.hpp"

#include <esp_err.h>
#include <esp_http_server.h>

namespace EspHttpServer
{
class HttpServer
{
public:
    esp_err_t start(uint16_t port = 80);
    esp_err_t stop();

    bool isRunning() const { return running_; }

    httpd_handle_t& getNativeHandle() { return server_; }

    esp_err_t registerUri(HttpUri& uri);
    esp_err_t unregisterUri(HttpUri& uri);

private:
    httpd_handle_t server_;
    httpd_config_t config_;
    bool running_{ false };
};
}

#endif  //_ESP_HTTP_SERVER_CXX_HTTP_SERVER_HPP
