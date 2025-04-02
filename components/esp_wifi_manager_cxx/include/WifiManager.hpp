#ifndef _ESP_WIFI_MANAGER_CXX_WIFI_MANAGER_HPP
#define _ESP_WIFI_MANAGER_CXX_WIFI_MANAGER_HPP

#include "WifiConfig.hpp"

#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

namespace EspWifiManager
{
template<typename ConfigT> class WifiManager
{
private:
    inline static constexpr const char* TAG = "WifiManager";

public:
    explicit WifiManager(ConfigT config)
        : config_{ config }
    {
        if (!wifiInitialized_)
        {
            initWifi();
        }

        setupNetif();
        config_.apply();
        ESP_ERROR_CHECK(esp_wifi_start());
        config_.run();
    }

    ~WifiManager()
    {
        config_.disable();
        ESP_ERROR_CHECK(esp_wifi_stop());

        cleanupNetif();

        if (wifiInitialized_)
        {
            deinitWifi();
        }
    }

private:
    static void initWifi()
    {
        ESP_LOGI(TAG, "Initializing WiFi...");

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        /*ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));*/

        wifiInitialized_ = true;
    }

    static void deinitWifi()
    {
        ESP_LOGI(TAG, "Deinitializing WiFi...");
        ESP_ERROR_CHECK(esp_wifi_deinit());
        wifiInitialized_ = false;
    }

    void setupNetif()
    {
        ESP_LOGI(TAG, "Setting up network interface...");

        if constexpr (std::is_same_v<ConfigT, WifiConfigAP>)
        {
            apNetif_ = esp_netif_create_default_wifi_ap();
        }
        else if constexpr (std::is_same_v<ConfigT, WifiConfigStation>)
        {
            staNetif_ = esp_netif_create_default_wifi_sta();
        }
        else if constexpr (std::is_same_v<ConfigT, WifiConfigAPandStation>)
        {
            apNetif_ = esp_netif_create_default_wifi_ap();
            staNetif_ = esp_netif_create_default_wifi_sta();
        }
        else if constexpr (std::is_same_v<ConfigT, WifiConfigScanner>)
        {
            staNetif_ = esp_netif_create_default_wifi_sta();
        }
    }

    void cleanupNetif()
    {
        ESP_LOGI(TAG, "Destroying network interfaces...");

        if (apNetif_)
        {
            esp_netif_destroy(apNetif_);
            apNetif_ = nullptr;
        }

        if (staNetif_)
        {
            esp_netif_destroy(staNetif_);
            staNetif_ = nullptr;
        }
    }

private:
    ConfigT config_;
    esp_netif_t* apNetif_ = nullptr;
    esp_netif_t* staNetif_ = nullptr;
    static inline bool wifiInitialized_ = false;
};
}

#endif  //_ESP_WIFI_MANAGER_CXX_WIFI_MANAGER_HPP
