#include "WifiProvisioningWeb.hpp"
#include "FormParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <memory>

#include <esp_http_server.h>

namespace EspWifiProvisioningWeb
{
extern const uint8_t
    wifi_login_html_start[] asm("_binary_wifi_login_html_start");
extern const uint8_t wifi_login_html_end[] asm("_binary_wifi_login_html_end");

WifiProvisioningWeb::WifiProvisioningWeb(
    WifiManager& wifiManager, HttpServer& httpServer)
    : wifiManager_{ wifiManager }
    , httpServer_{ httpServer }
    , wifiSignInPageUri_{ "/",
                          HTTP_GET,
                          [](HttpRequest req) -> HttpResponse
                          {
                              HttpResponse response(req);
                              response.setStatus("200 OK");
                              const size_t wifi_login_html_size
                                  = wifi_login_html_end - wifi_login_html_start;
                              response.setContent(
                                  std::string_view{
                                      reinterpret_cast<const char*>(
                                          wifi_login_html_start),
                                      wifi_login_html_size },
                                  "text/html");
                              return response;
                          } }
    , wifiConnectUri_{
        "/connect",
        HTTP_POST,
        [this](HttpRequest req) -> HttpResponse
        {
            HttpResponse response(req);
            std::string content = req.getContent();
            ESP_LOGI(TAG, "Received body: %s", content.c_str());
            FormParser parser{ content };
            auto ssid = parser.get("ssid");
            auto pass = parser.get("password");
            if (!ssid.has_value() or !pass.has_value())
            {
                response.setStatus("400 Bad Request");
                response.setContent("Invalid Data", "text/plain");
                ESP_LOGW(TAG, "Invalid Request Content");
            }
            else
            {
                ssid_ = ssid.value();
                pass_ = pass.value();
                ESP_LOGI(TAG, "SSID: %s", std::string{ ssid.value() }.c_str());
                ESP_LOGI(
                    TAG, "Password: %s", std::string{ pass.value() }.c_str());
                response.setStatus("200 OK");
                response.setContent("Trying to connect to AP", "text/plain");
                // try to connect
                xTaskCreate(
                    &wifiConnect,
                    "WifiProvConnectTask",
                    4096,
                    this,
                    tskIDLE_PRIORITY + 1,
                    &wifiConnectTaskHandle_);
            }
            return response;
        }
    }
{
}

WifiProvisioningWeb::~WifiProvisioningWeb() { stop(); }

void WifiProvisioningWeb::start(
    std::string_view apSsid, std::string_view apPass)
{
    apSsid_ = apSsid;
    apPass_ = apPass;
    configureWifiAp();

    configureHttpServer();
}

void WifiProvisioningWeb::stop()
{
    httpServer_.stop();
    wifiManager_.setConfig(nullptr);
}

void WifiProvisioningWeb::configureHttpServer()
{
    ESP_ERROR_CHECK(httpServer_.start());
    httpServer_.registerUri(wifiSignInPageUri_);
    httpServer_.registerUri(wifiConnectUri_);
}

void WifiProvisioningWeb::configureWifiAp()
{
    wifiManager_.setConfig(std::make_unique<WifiConfigAP>(apSsid_, apPass_));
}

void WifiProvisioningWeb::deconfigureHttpServer()
{
    ESP_ERROR_CHECK(httpServer_.stop());
}

void WifiProvisioningWeb::wifiConnect(void* arg)
{
    WifiProvisioningWeb& wifiProv = *static_cast<WifiProvisioningWeb*>(arg);
    size_t retry = 0;
    volatile bool provisioningFinished = false;
    volatile bool provisioningSuccessful = false;
    size_t maxRetries = 3;
    wifiProv.wifiManager_.setConfig(std::make_unique<WifiConfigStation>(
        wifiProv.ssid_,
        wifiProv.pass_,
        [&wifiProv, &provisioningFinished, &provisioningSuccessful](
            std::string ip)
        {
            provisioningSuccessful = true;
            provisioningFinished = true;
            xTaskNotifyGive(wifiProv.wifiConnectTaskHandle_);
        },
        [&wifiProv,
         &retry,
         &maxRetries,
         &provisioningSuccessful,
         &provisioningFinished](uint8_t reason)
        {
            switch (reason)
            {
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGW(TAG, "Requested AP not found");
                retry++;
                break;
            case WIFI_REASON_AUTH_FAIL:
                ESP_LOGW(TAG, "Invalid password");
                provisioningSuccessful = false;
                provisioningFinished = true;
                xTaskNotifyGive(wifiProv.wifiConnectTaskHandle_);
                break;
            default:
                retry++;
                break;
            }
            if (!provisioningFinished)
            {
                if (retry < maxRetries)
                {
                    esp_wifi_connect();
                }
                else
                {
                    provisioningSuccessful = false;
                    provisioningFinished = true;
                    xTaskNotifyGive(wifiProv.wifiConnectTaskHandle_);
                }
            }
        }));
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    wifiProv.deconfigureHttpServer();
    if (provisioningSuccessful)
    {
        wifiProv.isProvisioned_ = true;
    }
    else
    {
        wifiProv.start(wifiProv.apSsid_, wifiProv.apPass_);
    }
    vTaskDelete(nullptr);
}
}
