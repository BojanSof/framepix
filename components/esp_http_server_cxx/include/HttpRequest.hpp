#ifndef _ESP_HTTP_SERVER_CXX_HTTP_REQUEST_HPP
#define _ESP_HTTP_SERVER_CXX_HTTP_REQUEST_HPP

#include <esp_http_server.h>

#include <string>
#include <string_view>

namespace EspHttpServer
{
class HttpRequest
{
public:
    HttpRequest(httpd_req_t* req)
        : req_{ req }
    {
    }

    httpd_method_t getMethod() const
    {
        return static_cast<httpd_method_t>(req_->method);
    }

    std::string_view getUri() const { return std::string_view{ req_->uri }; }

    std::string getHeader(const char* key) const
    {
        const uint16_t bufLen = httpd_req_get_hdr_value_len(req_, key);
        if (bufLen > 0)
        {
            std::string value(bufLen, ' ');
            if (httpd_req_get_hdr_value_str(req_, key, value.data(), bufLen)
                == ESP_OK)
            {
                return value;
            }
        }
        return std::string{ "" };
    }

    std::string getBody()
    {
        if (req_->content_len > 0)
        {
            std::string body(req_->content_len, ' ');
            int received = httpd_req_recv(req_, body.data(), req_->content_len);
            if (received > 0)
            {
                return body;
            }
        }
        return std::string{ "" };
    }

    httpd_req_t* getNativeHandle() { return req_; }

private:
    httpd_req_t* req_;
};
}  // namespace EspHttpServer

#endif  //_ESP_HTTP_SERVER_CXX_HTTP_REQUEST_HPP
