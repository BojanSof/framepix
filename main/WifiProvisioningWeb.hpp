#ifndef WIFI_PROVISIONING_WEB_HPP
#define WIFI_PROVISIONING_WEB_HPP

#include "HttpServer.hpp"
#include "WifiManager.hpp"

#include <string_view>

namespace EspWifiProvisioningWeb
{
using namespace EspWifiManager;
using namespace EspHttpServer;

class WifiProvisioningWeb
{
private:
    inline static constexpr const char* TAG = "WifiProvisioningWeb";

public:
    WifiProvisioningWeb(WifiManager& wifiManager, HttpServer& httpServer);
    ~WifiProvisioningWeb();

    void start(std::string_view apSsid, std::string_view apPass);
    void stop();

private:
    WifiManager& wifiManager_;
    HttpServer& httpServer_;
    HttpUri wifiSignInPageUri_;
    HttpUri wifiConnectUri_;
};
}

#endif  // WIFI_PROVISIONING_WEB_HPP
