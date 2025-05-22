#ifndef _ESP_HTTP_SERVER_CXX_HTTP_REQUEST_HPP
#define _ESP_HTTP_SERVER_CXX_HTTP_REQUEST_HPP

#include <esp_http_server.h>

#include <optional>
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

    std::optional<std::string> getHeader(const char* key) const
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
        return std::nullopt;
    }

    std::optional<std::string> getQueryParam(const char* key) const
    {
        std::string_view uri = getUri();
        size_t queryStart = uri.find('?');
        if (queryStart == std::string_view::npos)
        {
            return std::nullopt;
        }

        std::string_view query = uri.substr(queryStart + 1);
        std::string keyStr = std::string(key) + "=";
        size_t paramStart = query.find(keyStr);

        if (paramStart == std::string_view::npos)
        {
            return std::nullopt;
        }

        size_t valueStart = paramStart + keyStr.length();
        size_t valueEnd = query.find('&', valueStart);

        if (valueEnd == std::string_view::npos)
        {
            return std::string(query.substr(valueStart));
        }

        return std::string(query.substr(valueStart, valueEnd - valueStart));
    }

    std::string getContent()
    {
        if (req_->content_len > 0)
        {
            std::string content(req_->content_len, ' ');
            int received = 0;
            while (received < req_->content_len)
            {

                int ret = httpd_req_recv(
                    req_,
                    content.data() + received,
                    req_->content_len - received);
                if (ret < 0)
                {
                    return std::string{ "" };
                }
                else
                {
                    received += ret;
                }
            }
            return content;
        }
        return std::string{ "" };
    }

    httpd_req_t* getNativeHandle() { return req_; }

private:
    httpd_req_t* req_;
};
}  // namespace EspHttpServer

#endif  //_ESP_HTTP_SERVER_CXX_HTTP_REQUEST_HPP
