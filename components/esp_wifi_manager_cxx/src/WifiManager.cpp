#include "WifiManager.hpp"

namespace EspWifiManager
{
WifiManager::WifiManager(std::unique_ptr<WifiConfig> config)
    : config_(std::move(config))
{
    if (!wifiInitialized_)
    {
        initWifi();
    }

    if (config_)
    {
        setupNetif();
        config_->apply();
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}

WifiManager::~WifiManager()
{
    if (config_)
    {
        config_->disable();
        ESP_ERROR_CHECK(esp_wifi_stop());
    }

    cleanupNetif();

    if (wifiInitialized_)
    {
        deinitWifi();
    }
}

void WifiManager::initWifi()
{
    ESP_LOGI(TAG, "Initializing WiFi...");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /*ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));*/

    wifiInitialized_ = true;
}

void WifiManager::deinitWifi()
{
    ESP_LOGI(TAG, "Deinitializing WiFi...");
    ESP_ERROR_CHECK(esp_wifi_deinit());
    wifiInitialized_ = false;
}

void WifiManager::setupNetif()
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

void WifiManager::cleanupNetif()
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
}
