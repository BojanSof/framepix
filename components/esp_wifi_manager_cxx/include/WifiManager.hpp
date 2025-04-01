#ifndef _ESP_WIFI_MANAGER_CXX
#define _ESP_WIFI_MANAGER_CXX

#include "WifiConfig.hpp"

#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <memory>

namespace EspWifiManager
{
class WifiManager
{
private:
    inline static constexpr const char* TAG = "WifiManager";

public:
    explicit WifiManager(std::unique_ptr<WifiConfig> config);
    ~WifiManager();

private:
    static void initWifi();
    static void deinitWifi();
    void setupNetif();
    void cleanupNetif();

    std::unique_ptr<WifiConfig> config_;
    esp_netif_t* apNetif_ = nullptr;
    esp_netif_t* staNetif_ = nullptr;
    static inline bool wifiInitialized_ = false;
};
}

#endif  //_ESP_WIFI_MANAGER_CXX
