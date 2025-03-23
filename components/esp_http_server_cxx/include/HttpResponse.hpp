#ifndef _ESP_HTTP_SERVER_CXX_HTTP_RESPONSE_HPP
#define _ESP_HTTP_SERVER_CXX_HTTP_RESPONSE_HPP

#include "HttpRequest.hpp"

#include <esp_http_server.h>

#include <string>
#include <string_view>

namespace EspHttpServer
{
class HttpResponse
{
public:
    HttpResponse(HttpRequest& req)
        : req_(req.getNativeHandle())
    {
    }

    void setStatus(const char* status) { httpd_resp_set_status(req_, status); }

    void setHeader(const char* key, const char* value)
    {
        httpd_resp_set_hdr(req_, key, value);
    }

    void
    setBody(const std::string_view body, const std::string_view contentType)
    {
        content_ = body;
        contentType_ = contentType;
    }

private:
    esp_err_t send()
    {
        esp_err_t err;
        err = httpd_resp_set_type(req_, contentType_.c_str());
        if (err != ESP_OK)
        {
            return err;
        }
        err = httpd_resp_send(req_, content_.c_str(), content_.size());
        return err;
    }

private:
    friend class HttpUri;

private:
    httpd_req_t* req_;
    std::string content_;
    std::string contentType_;
};

}  // namespace EspHttpServer

#endif  //_ESP_HTTP_SERVER_CXX_HTTP_RESPONSE_HPP
