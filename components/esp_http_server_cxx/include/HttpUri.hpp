#ifndef _ESP_HTTP_SERVER_CXX_HTTP_URI_HPP
#define _ESP_HTTP_SERVER_CXX_HTTP_URI_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <esp_http_server.h>

#include <functional>

namespace EspHttpServer
{
class HttpUri
{
public:
    using HttpUriHandlerType = std::function<HttpResponse(HttpRequest)>;

    HttpUri(const char* uri, http_method method, HttpUriHandlerType handler)
        : handler_{ handler }
        , uri_{ .uri = uri,
                .method = method,
                .handler =
                    [](httpd_req_t* req)
                {
                    HttpUri* uriObj = (HttpUri*)req->user_ctx;
                    auto response = uriObj->handler_(HttpRequest(req));
                    return response.send();
                },
                .user_ctx = this }
    {
    }

    HttpUri(HttpUri&& other) noexcept
        : handler_{ std::move(other.handler_) }
        , uri_{ other.uri_ }
    {
        uri_.user_ctx = this;
    }

    HttpUri& operator=(HttpUri&& other) noexcept
    {
        if (this != &other)
        {
            handler_ = std::move(other.handler_);
            uri_ = other.uri_;
            uri_.user_ctx = this;
        }
        return *this;
    }

    HttpUri(const HttpUri&) = delete;
    HttpUri& operator=(const HttpUri&) = delete;

    httpd_uri_t& getNativeHandle() { return uri_; }

private:
    HttpUriHandlerType handler_;
    httpd_uri_t uri_;
};
}

#endif  //_ESP_HTTP_SERVER_CXX_HTTP_URI_HANDLER_HPP
