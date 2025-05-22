#include "HttpServer.hpp"

#include <esp_log.h>

namespace EspHttpServer
{

static constexpr const char* TAG = "HttpServerCxx";

static HttpServer::Error fromEspErr(esp_err_t err)
{
    switch (err)
    {
    case ESP_OK:
        return HttpServer::Error::None;
    case ESP_ERR_INVALID_ARG:
        return HttpServer::Error::InvalidHandle;
    case ESP_ERR_INVALID_STATE:
        return HttpServer::Error::AlreadyRunning;
    case ESP_FAIL:
        return HttpServer::Error::StartFailed;
    default:
        return HttpServer::Error::Unknown;
    }
}

static const char* toString(EspHttpServer::HttpServer::Error err)
{
    using Error = EspHttpServer::HttpServer::Error;
    switch (err)
    {
    case Error::None:
        return "None";
    case Error::AlreadyRunning:
        return "AlreadyRunning";
    case Error::StartFailed:
        return "StartFailed";
    case Error::StopFailed:
        return "StopFailed";
    case Error::NotRunning:
        return "NotRunning";
    case Error::RegisterUriFailed:
        return "RegisterUriFailed";
    case Error::UnregisterUriFailed:
        return "UnregisterUriFailed";
    case Error::InvalidHandle:
        return "InvalidHandle";
    case Error::Unknown:
        return "Unknown";
    default:
        return "InvalidErrorCode";
    }
}

static void logResult(
    const char* action,
    esp_err_t err,
    EspHttpServer::HttpServer::Error mappedErr,
    const char* uri = nullptr)
{
    const char* resultStr = toString(mappedErr);

    if (mappedErr == EspHttpServer::HttpServer::Error::None)
    {
        if (uri)
            ESP_LOGI(TAG, "%s [%s]: OK", action, uri);
        else
            ESP_LOGI(TAG, "%s: OK", action);
    }
    else if (
        mappedErr == EspHttpServer::HttpServer::Error::AlreadyRunning
        || mappedErr == EspHttpServer::HttpServer::Error::NotRunning)
    {
        if (uri)
            ESP_LOGW(
                TAG,
                "%s [%s]: %s (esp_err_t=0x%x)",
                action,
                uri,
                resultStr,
                err);
        else
            ESP_LOGW(TAG, "%s: %s (esp_err_t=0x%x)", action, resultStr, err);
    }
    else
    {
        if (uri)
            ESP_LOGE(
                TAG,
                "%s [%s]: %s (esp_err_t=0x%x)",
                action,
                uri,
                resultStr,
                err);
        else
            ESP_LOGE(TAG, "%s: %s (esp_err_t=0x%x)", action, resultStr, err);
    }
}

HttpServer::Error HttpServer::start(uint16_t port)
{
    if (running_)
    {
        Error err = Error::AlreadyRunning;
        logResult("Start", ESP_ERR_INVALID_STATE, err);
        return err;
    }

    config_ = HTTPD_DEFAULT_CONFIG();
    config_.server_port = port;
    config_.stack_size = 10240;
    config_.max_uri_handlers = 20;

    esp_err_t err = httpd_start(&server_, &config_);
    running_ = (err == ESP_OK);
    Error result = fromEspErr(err);
    logResult("Start", err, result);
    return result;
}

HttpServer::Error HttpServer::stop()
{
    if (!running_)
    {
        Error err = Error::NotRunning;
        logResult("Stop", ESP_ERR_INVALID_STATE, err);
        return err;
    }

    esp_err_t err = httpd_stop(server_);
    running_ = false;

    Error result = (err == ESP_OK) ? Error::None : Error::StopFailed;
    logResult("Stop", err, result);
    return result;
}

HttpServer::Error HttpServer::registerUri(HttpUri& uri)
{
    esp_err_t err
        = httpd_register_uri_handler(getNativeHandle(), &uri.getNativeHandle());
    Error result = (err == ESP_OK) ? Error::None : Error::RegisterUriFailed;
    logResult("Register URI", err, result, uri.getNativeHandle().uri);
    return result;
}

HttpServer::Error HttpServer::unregisterUri(HttpUri& uri)
{
    esp_err_t err
        = httpd_unregister_uri(getNativeHandle(), uri.getNativeHandle().uri);
    Error result = (err == ESP_OK) ? Error::None : Error::UnregisterUriFailed;
    logResult("Unregister URI", err, result, uri.getNativeHandle().uri);
    return result;
}

}  // namespace EspHttpServer
