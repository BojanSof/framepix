#ifndef _ESP_WIFI_MANAGER_CXX_WIFI_MANAGER_HPP
#define _ESP_WIFI_MANAGER_CXX_WIFI_MANAGER_HPP

#include "WifiConfig.hpp"

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
    explicit WifiManager(std::unique_ptr<WifiConfig> config = nullptr);

    ~WifiManager();

    void setConfig(std::unique_ptr<WifiConfig> config);

    WifiMode getMode() const;

private:
    static void initWifi();
    static void deinitWifi();

    void setupNetif();

    void cleanupNetif();

private:
    std::unique_ptr<WifiConfig> config_;
    esp_netif_t* apNetif_ = nullptr;
    esp_netif_t* staNetif_ = nullptr;
    static inline bool wifiInitialized_ = false;
};
}

#endif  //_ESP_WIFI_MANAGER_CXX_WIFI_MANAGER_HPP
