#ifndef _ESP_WIFI_MANAGER_CXX
#define _ESP_WIFI_MANAGER_CXX

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

        wifi_mode_t mode;
        ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));

        if (mode == WIFI_MODE_AP)
        {
            apNetif_ = esp_netif_create_default_wifi_ap();
        }
        else if (mode == WIFI_MODE_STA)
        {
            staNetif_ = esp_netif_create_default_wifi_sta();
        }
        else if (mode == WIFI_MODE_APSTA)
        {
            apNetif_ = esp_netif_create_default_wifi_ap();
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

#endif  //_ESP_WIFI_MANAGER_CXX
