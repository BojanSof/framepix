#ifndef WIFI_PROVISIONING_WEB_HPP
#define WIFI_PROVISIONING_WEB_HPP

#include "HttpServer.hpp"
#include "WifiManager.hpp"

#include "freertos/task.h"

#include <functional>
#include <string_view>

namespace EspWifiProvisioningWeb
{
using namespace EspWifiManager;
using namespace EspHttpServer;

class WifiProvisioningWeb
{
public:
    enum class FailReason
    {
        APNotFound,
        InvalidAPPassword
    };

private:
    inline static constexpr const char* TAG = "WifiProvisioningWeb";

public:
    using OnProvisioned = std::function<void()>;
    using OnProvisionFailed = std::function<void(FailReason)>;

public:
    WifiProvisioningWeb(WifiManager& wifiManager, HttpServer& httpServer);
    ~WifiProvisioningWeb();

    void start(
        std::string_view apSsid,
        std::string_view apPass,
        OnProvisioned onProvisioned = nullptr,
        OnProvisionFailed onProvisionFailed = nullptr);
    void stop();

private:
    void configureHttpServer();
    void deconfigureHttpServer();
    void configureWifiAp();

private:
    static void wifiConnect(void* arg);

private:
    WifiManager& wifiManager_;
    HttpServer& httpServer_;
    HttpUri wifiSignInPageUri_;
    HttpUri wifiSignInPageCssUri_;
    HttpUri wifiConnectUri_;
    TaskHandle_t wifiConnectTaskHandle_{ nullptr };
    OnProvisioned onProvisioned_{ nullptr };
    OnProvisionFailed onProvisionFailed_{ nullptr };

    // for connecting to AP
    std::string_view apSsid_;
    std::string_view apPass_;
    std::string_view ssid_;
    std::string_view pass_;

    // status
    bool isProvisioned_{ false };
};
}

#endif  // WIFI_PROVISIONING_WEB_HPP
