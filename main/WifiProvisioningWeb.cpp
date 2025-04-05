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
        [](HttpRequest req) -> HttpResponse
        {
            HttpResponse response(req);
            std::string content = req.getContent();
            ESP_LOGI(TAG, "Received body: %s", content.c_str());
            // Parse the body to extract SSID and password
            // (ssid=xxx&pass=xxx)
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

                ESP_LOGI(TAG, "SSID: %s", std::string{ ssid.value() }.c_str());
                ESP_LOGI(
                    TAG, "Password: %s", std::string{ pass.value() }.c_str());
                response.setStatus("200 OK");
                response.setContent("OK", "text/plain");
            }
            /*wifiManager_.setConfig(*/
            /*    std::make_unique<WifiConfigStation>(ssid,
             * pass));*/
            return response;
        }
    }
{
}

WifiProvisioningWeb::~WifiProvisioningWeb() { stop(); }

void WifiProvisioningWeb::start(
    std::string_view apSsid, std::string_view apPass)
{
    wifiManager_.setConfig(std::make_unique<WifiConfigAP>(apSsid, apPass));
    ESP_ERROR_CHECK(httpServer_.start());

    httpServer_.registerUri(wifiSignInPageUri_);
    httpServer_.registerUri(wifiConnectUri_);
}

void WifiProvisioningWeb::stop()
{
    httpServer_.stop();
    wifiManager_.setConfig(nullptr);
}
}
