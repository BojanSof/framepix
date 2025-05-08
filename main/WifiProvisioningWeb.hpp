#ifndef WIFI_PROVISIONING_WEB_HPP
#define WIFI_PROVISIONING_WEB_HPP

#include "HttpServer.hpp"
#include "WifiManager.hpp"

#include "Spiffs.hpp"

#include "freertos/task.h"

#include <functional>
#include <string_view>

namespace EspWifiProvisioningWeb
{
using namespace EspWifiManager;
using namespace EspHttpServer;

struct WifiCredentials
{
    static constexpr size_t MaxSsidLen = 32;
    static constexpr size_t MaxPasswordLen = 64;

    std::string ssid;
    std::string password;
};

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
    WifiProvisioningWeb(
        WifiManager& wifiManager, HttpServer& httpServer, Spiffs& spiffs);
    ~WifiProvisioningWeb();

    void start(
        std::string_view apSsid,
        std::string_view apPass,
        OnProvisioned onProvisioned = nullptr,
        OnProvisionFailed onProvisionFailed = nullptr);
    void stop();
    bool checkForPreviousProvisioning();
    bool applyPreviousProvisioning(
        OnProvisioned onProvisioned = nullptr,
        OnProvisionFailed onProvisionFailed = nullptr);
    bool removePreviousProvisioning();

private:
    void configureHttpServer();
    void deconfigureHttpServer();
    void configureWifiAp();

private:
    static void wifiConnect(void* arg);

private:
    WifiManager& wifiManager_;
    HttpServer& httpServer_;
    Spiffs& spiffs_;
    HttpUri wifiSignInPageUri_;
    HttpUri wifiSignInPageCssUri_;
    HttpUri wifiConnectUri_;
    TaskHandle_t wifiConnectTaskHandle_{ nullptr };
    OnProvisioned onProvisioned_{ nullptr };
    OnProvisionFailed onProvisionFailed_{ nullptr };

    WifiCredentials apCredentials_;
    WifiCredentials stationCredentials_;
    bool checkingStoredCredentials_{ false };

    static constexpr const char* wifiCredentialsFile{ "wificreds.bin" };

    // status
    bool isProvisioned_{ false };
};
}

#endif  // WIFI_PROVISIONING_WEB_HPP
