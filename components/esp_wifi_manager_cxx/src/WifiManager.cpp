#include "WifiManager.hpp"

#include <esp_log.h>

namespace EspWifiManager
{
WifiManager::WifiManager(std::unique_ptr<WifiConfig> config)
    : config_{ std::move(config) }
{
    if (!wifiInitialized_)
    {
        initWifi();
    }
    if (config_)
    {
        setupNetif();
        config_->apply();
        esp_wifi_start();
        esp_wifi_set_ps(WIFI_PS_NONE);
        config_->run();
    }
}

WifiManager::~WifiManager()
{
    if (config_)
    {
        config_->disable();
        esp_wifi_stop();
        cleanupNetif();
    }

    if (wifiInitialized_)
    {
        deinitWifi();
    }
}

void WifiManager::setConfig(std::unique_ptr<WifiConfig> config)
{
    if (config_)
    {
        config_->disable();
        esp_wifi_stop();
        cleanupNetif();
    }
    if (config)
    {
        config_ = std::move(config);
        setupNetif();
        config_->apply();
        esp_wifi_start();
        esp_wifi_set_ps(WIFI_PS_NONE);
        config_->run();
    }
    else
    {
        config_ = nullptr;
    }
}

WifiMode WifiManager::getMode() const
{
    if (config_)
    {
        return config_->getMode();
    }
    return WifiMode::Null;
}

void WifiManager::initWifi()
{
    ESP_LOGI(TAG, "Initializing WiFi...");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifiInitialized_ = true;
}

void WifiManager::deinitWifi()
{
    ESP_LOGI(TAG, "Deinitializing WiFi...");
    esp_wifi_deinit();
    wifiInitialized_ = false;
}

void WifiManager::setupNetif()
{
    if (!config_)
    {
        return;
    }
    ESP_LOGI(TAG, "Setting up network interface...");

    if (config_->getMode() == WifiMode::AP)
    {
        apNetif_ = esp_netif_create_default_wifi_ap();
    }
    else if (config_->getMode() == WifiMode::Station)
    {
        staNetif_ = esp_netif_create_default_wifi_sta();
    }
    else if (config_->getMode() == WifiMode::APandStation)
    {
        apNetif_ = esp_netif_create_default_wifi_ap();
        staNetif_ = esp_netif_create_default_wifi_sta();
    }
    else if (config_->getMode() == WifiMode::Scanner)
    {
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
