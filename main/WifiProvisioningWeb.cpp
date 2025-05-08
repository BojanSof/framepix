#include "WifiProvisioningWeb.hpp"
#include "FormParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <memory>
#include <optional>

#include <esp_http_server.h>

namespace EspWifiProvisioningWeb
{
extern const uint8_t
    wifi_login_html_start[] asm("_binary_wifi_login_html_start");
extern const uint8_t wifi_login_html_end[] asm("_binary_wifi_login_html_end");

extern const uint8_t css_styles_css_start[] asm("_binary_styles_css_start");
extern const uint8_t css_styles_css_end[] asm("_binary_styles_css_end");

struct WifiCredentialsSerializer
{
    using value_type = WifiCredentials;
    // length-value: [ssid_len][ssid_bytes][pass_len][pass_bytes]
    static constexpr size_t maxSize
        = 1 + WifiCredentials::MaxSsidLen + 1 + WifiCredentials::MaxPasswordLen;

    static size_t
    serialize(const WifiCredentials& cred, std::span<std::byte> buf) noexcept
    {
        // Ensure span is big enough
        if (buf.size() < maxSize)
        {
            return 0;
        }

        size_t ssidLen
            = std::min(cred.ssid.size(), WifiCredentials::MaxSsidLen);
        size_t pwdLen
            = std::min(cred.password.size(), WifiCredentials::MaxPasswordLen);

        size_t offset = 0;
        buf[offset++] = static_cast<std::byte>(ssidLen);
        std::memcpy(buf.data() + offset, cred.ssid.data(), ssidLen);
        offset += ssidLen;

        buf[offset++] = static_cast<std::byte>(pwdLen);
        std::memcpy(buf.data() + offset, cred.password.data(), pwdLen);
        offset += pwdLen;

        return offset;
    }

    static std::optional<WifiCredentials>
    deserialize(std::span<const std::byte> buf) noexcept
    {
        if (buf.size() < 2)
        {
            return std::nullopt;
        }

        size_t offset = 0;
        size_t ssidLen = static_cast<size_t>(buf[offset++]);
        if (offset + ssidLen + 1 > buf.size())
        {
            return std::nullopt;
        }

        auto ssidPtr = reinterpret_cast<const char*>(buf.data() + offset);
        std::string_view ssid{ ssidPtr, ssidLen };
        offset += ssidLen;

        size_t pwdLen = static_cast<size_t>(buf[offset++]);
        if (offset + pwdLen > buf.size())
        {
            return std::nullopt;
        }

        auto pwdPtr = reinterpret_cast<const char*>(buf.data() + offset);
        std::string_view pwd{ pwdPtr, pwdLen };

        return WifiCredentials{ std::string{ ssid }, std::string{ pwd } };
    }
};

WifiProvisioningWeb::WifiProvisioningWeb(
    WifiManager& wifiManager, HttpServer& httpServer, Spiffs& spiffs)
    : wifiManager_{ wifiManager }
    , httpServer_{ httpServer }
    , spiffs_{ spiffs }
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
    , wifiSignInPageCssUri_{ "/css/styles.css",
                             HTTP_GET,
                             [](HttpRequest req) -> HttpResponse
                             {
                                 HttpResponse response(req);
                                 response.setStatus("200 OK");
                                 size_t css_size = css_styles_css_end
                                     - css_styles_css_start;
                                 response.setContent(
                                     std::string_view{
                                         reinterpret_cast<const char*>(
                                             css_styles_css_start),
                                         css_size },
                                     "text/css");
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
                stationCredentials_.ssid = ssid.value();
                stationCredentials_.password = pass.value();
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
    std::string_view apSsid,
    std::string_view apPass,
    OnProvisioned onProvisioned,
    OnProvisionFailed onProvisionFailed)
{
    apCredentials_.ssid = apSsid;
    apCredentials_.password = apPass;
    onProvisioned_ = onProvisioned;
    onProvisionFailed_ = onProvisionFailed;
    configureWifiAp();

    configureHttpServer();
}

void WifiProvisioningWeb::stop()
{
    httpServer_.stop();
    wifiManager_.setConfig(nullptr);
}

bool WifiProvisioningWeb::checkForPreviousProvisioning()
{
    return spiffs_.exists(wifiCredentialsFile).value_or(false);
}

bool WifiProvisioningWeb::applyPreviousProvisioning(
    OnProvisioned onProvisioned, OnProvisionFailed onProvisionFailed)
{
    // apply previous provisioning
    // read from spiffs
    std::array<std::byte, WifiCredentialsSerializer::maxSize> buffer{};
    auto we = spiffs_.readObject<WifiCredentialsSerializer, WifiCredentials>(
        wifiCredentialsFile, std::span{ buffer });
    if (!we)
    {
        ESP_LOGE(
            TAG,
            "Failed to read credentials: %d",
            static_cast<int>(we.error()));
        return false;
    }
    auto cred = we.value();
    if (cred.ssid.empty() || cred.password.empty())
    {
        ESP_LOGE(TAG, "Invalid credentials");
        removePreviousProvisioning();
        return false;
    }
    stationCredentials_.ssid = cred.ssid;
    stationCredentials_.password = cred.password;
    ESP_LOGI(TAG, "SSID: %s", stationCredentials_.ssid.c_str());
    ESP_LOGI(TAG, "Password: %s", stationCredentials_.password.c_str());
    onProvisioned_ = onProvisioned;
    onProvisionFailed_ = onProvisionFailed;
    checkingStoredCredentials_ = true;
    xTaskCreate(
        &wifiConnect,
        "WifiProvConnectTask",
        4096,
        this,
        tskIDLE_PRIORITY + 1,
        &wifiConnectTaskHandle_);
    return true;
}

bool WifiProvisioningWeb::removePreviousProvisioning()
{
    if (auto we = spiffs_.remove(wifiCredentialsFile); !we)
    {
        ESP_LOGE(
            TAG,
            "Failed to remove credentials file: %d",
            static_cast<int>(we.error()));
        return false;
    }
    return true;
}

void WifiProvisioningWeb::configureHttpServer()
{
    ESP_ERROR_CHECK(httpServer_.start());
    httpServer_.registerUri(wifiSignInPageUri_);
    httpServer_.registerUri(wifiSignInPageCssUri_);
    httpServer_.registerUri(wifiConnectUri_);
}

void WifiProvisioningWeb::configureWifiAp()
{
    wifiManager_.setConfig(std::make_unique<WifiConfigAP>(
        apCredentials_.ssid, apCredentials_.password));
}

void WifiProvisioningWeb::deconfigureHttpServer()
{
    if (httpServer_.isRunning())
    {
        ESP_ERROR_CHECK(httpServer_.stop());
    }
}

void WifiProvisioningWeb::wifiConnect(void* arg)
{
    WifiProvisioningWeb& wifiProv = *static_cast<WifiProvisioningWeb*>(arg);
    size_t retry = 0;
    volatile bool provisioningFinished = false;
    volatile bool provisioningSuccessful = false;
    FailReason failReason;
    size_t maxRetries = 3;
    wifiProv.wifiManager_.setConfig(std::make_unique<WifiConfigStation>(
        wifiProv.stationCredentials_.ssid,
        wifiProv.stationCredentials_.password,
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
         &provisioningFinished,
         &failReason](uint8_t reason)
        {
            switch (reason)
            {
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGW(TAG, "Requested AP not found");
                failReason = FailReason::APNotFound;
                retry++;
                break;
            case WIFI_REASON_AUTH_FAIL:
                ESP_LOGW(TAG, "Invalid password");
                failReason = FailReason::InvalidAPPassword;
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
        // save ssid and password to spiffs
        std::array<std::byte, WifiCredentialsSerializer::maxSize> buffer{};
        if (auto we
            = wifiProv.spiffs_
                  .writeObject<WifiCredentialsSerializer, WifiCredentials>(
                      wifiProv.wifiCredentialsFile,
                      wifiProv.stationCredentials_,
                      std::span{ buffer });
            !we)
        {
            ESP_LOGE(
                TAG,
                "Failed to save credentials: %d",
                static_cast<int>(we.error()));
        }
        if (wifiProv.onProvisioned_)
        {
            wifiProv.onProvisioned_();
        }
    }
    else
    {
        if (wifiProv.checkingStoredCredentials_)
        {
            // remove the file
            wifiProv.removePreviousProvisioning();
            wifiProv.checkingStoredCredentials_ = false;
        }
        if (wifiProv.onProvisionFailed_)
        {
            wifiProv.onProvisionFailed_(failReason);
        }
        wifiProv.start(
            wifiProv.apCredentials_.ssid,
            wifiProv.apCredentials_.password,
            wifiProv.onProvisioned_,
            wifiProv.onProvisionFailed_);
    }
    vTaskDelete(nullptr);
}
}
