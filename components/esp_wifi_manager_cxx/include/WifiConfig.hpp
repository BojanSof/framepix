#ifndef _ESP_WIFI_MANAGER_CXX_WIFI_CONFIG_HPP
#define _ESP_WIFI_MANAGER_CXX_WIFI_CONFIG_HPP

#include "esp_wifi_types_generic.h"
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include <cstring>
#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace EspWifiManager
{

enum class WifiMode
{
    Null,
    AP,
    Station,
    APandStation,
    Scanner
};

class WifiConfig
{
public:
    WifiConfig(const WifiMode& mode)
        : mode_{ mode }
    {
    }

    virtual ~WifiConfig() = default;
    virtual bool apply() = 0;
    virtual bool disable() = 0;

    WifiMode getMode() const { return mode_; }

    virtual bool run() { return true; }

protected:
    wifi_config_t config_{};
    WifiMode mode_{ WifiMode::Null };
};

class WifiConfigAP : public virtual WifiConfig
{
private:
    inline static constexpr const char* TAG = "WifiConfigAP";

public:
    using OnDeviceConnected
        = std::function<void(std::string mac, std::string ip)>;
    using OnDeviceDisconnected = std::function<void(std::string mac)>;

public:
    WifiConfigAP(
        std::string_view ssid,
        std::string_view pass,
        OnDeviceConnected onDeviceConnected = nullptr,
        OnDeviceDisconnected onDeviceDisconnected = nullptr,
        const uint8_t channel = 1,
        const uint8_t numConnections = 1)
        : WifiConfig(WifiMode::AP)
        , onDeviceConnected_{ onDeviceConnected }
        , onDeviceDisconnected_{ onDeviceDisconnected }
    {
        const size_t ssidLen = std::min(ssid.size(), sizeof(config_.ap.ssid));
        strncpy(reinterpret_cast<char*>(config_.ap.ssid), ssid.data(), ssidLen);
        config_.ap.ssid_len = ssidLen;

        const size_t passLen
            = std::min(pass.size(), sizeof(config_.ap.password));
        strncpy(
            reinterpret_cast<char*>(config_.ap.password), pass.data(), passLen);

        config_.ap.channel = channel;
        config_.ap.max_connection = numConnections;
        config_.ap.authmode
            = (passLen == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        config_.ap.pmf_cfg.required = true;
    }

    virtual bool apply() override
    {
        esp_err_t err;

        err = esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &wifiEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to register event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &ipEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to register IP event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_wifi_set_mode(WIFI_MODE_AP);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set wifi mode: %s", esp_err_to_name(err));
            return false;
        }

        err = esp_wifi_set_config(WIFI_IF_AP, &config_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG, "Failed to set wifi AP config: %s", esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "AP configuration applied successfully.");
        return true;
    }

    virtual bool disable() override
    {
        esp_err_t err;

        err = esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to unregister wifi event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_event_handler_instance_unregister(
            IP_EVENT, ESP_EVENT_ANY_ID, ipEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to unregister IP event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        wifi_mode_t mode;
        err = esp_wifi_get_mode(&mode);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get wifi mode: %s", esp_err_to_name(err));
            return false;
        }

        if (mode == WIFI_MODE_APSTA)
        {
            err = esp_wifi_set_mode(WIFI_MODE_STA);
        }
        else
        {
            err = esp_wifi_set_mode(WIFI_MODE_NULL);
        }
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to set wifi mode to NULL: %s",
                esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "Wifi AP disabled successfully.");
        return true;
    }

protected:
    static void eventHandler(
        void* arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data)
    {
        auto* apConfig = static_cast<WifiConfigAP*>(arg);
        if (event_base == WIFI_EVENT)
        {
            if (event_id == WIFI_EVENT_AP_STACONNECTED)
            {
                auto* event
                    = static_cast<wifi_event_ap_staconnected_t*>(event_data);
                ESP_LOGI(
                    TAG,
                    "station " MACSTR " join, AID=%d",
                    MAC2STR(event->mac),
                    event->aid);
            }
            else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
            {
                auto* event
                    = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
                ESP_LOGI(
                    TAG,
                    "station " MACSTR " leave, AID=%d, reason=%d",
                    MAC2STR(event->mac),
                    event->aid,
                    event->reason);
                std::string macClient
                    = std::format("{}{}{}{}{}{}", MAC2STR(event->mac));
                if (apConfig->onDeviceDisconnected_)
                {
                    apConfig->onDeviceDisconnected_(macClient);
                }
            }
        }
        else if (
            event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
        {
            auto* event = static_cast<ip_event_ap_staipassigned_t*>(event_data);
            if (apConfig->onDeviceConnected_)
            {
                std::string macClient
                    = std::format("{}{}{}{}{}{}", MAC2STR(event->mac));
                std::string ipClient
                    = std::format("{}.{}.{}.{}", IP2STR(&event->ip));
                apConfig->onDeviceConnected_(macClient, ipClient);
            }
        }
    }

protected:
    OnDeviceConnected onDeviceConnected_;
    OnDeviceDisconnected onDeviceDisconnected_;

private:
    esp_event_handler_instance_t wifiEventHandlerInstance_{};
    esp_event_handler_instance_t ipEventHandlerInstance_{};
};

class WifiConfigStation : public virtual WifiConfig
{
private:
    inline static constexpr const char* TAG = "WifiConfigStation";

public:
    using OnConnected = std::function<void(std::string ip)>;
    using OnDisconnected = std::function<void(uint8_t reason)>;

public:
    WifiConfigStation(
        std::string_view ssid,
        std::string_view pass,
        OnConnected onConnected = nullptr,
        OnDisconnected onDisconnected = nullptr)
        : WifiConfig(WifiMode::Station)
        , onConnected_{ onConnected }
        , onDisconnected_{ onDisconnected }
    {
        const size_t ssidLen = std::min(ssid.size(), sizeof(config_.sta.ssid));
        strncpy(
            reinterpret_cast<char*>(config_.sta.ssid), ssid.data(), ssidLen);

        const size_t passLen
            = std::min(pass.size(), sizeof(config_.sta.password));
        strncpy(
            reinterpret_cast<char*>(config_.sta.password),
            pass.data(),
            passLen);
    }

    virtual bool apply() override
    {
        esp_err_t err;

        err = esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &wifiEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to register event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &ipEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to register IP event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set wifi mode: %s", esp_err_to_name(err));
            return false;
        }

        err = esp_wifi_set_config(WIFI_IF_STA, &config_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG, "Failed to set wifi STA config: %s", esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "Station configuration applied successfully.");
        return true;
    }

    virtual bool disable() override
    {
        esp_err_t err;

        err = esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to unregister wifi event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_event_handler_instance_unregister(
            IP_EVENT, ESP_EVENT_ANY_ID, ipEventHandlerInstance_);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to unregister IP event handler: %s",
                esp_err_to_name(err));
            return false;
        }

        err = esp_wifi_set_mode(WIFI_MODE_NULL);
        if (err != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to set wifi mode to NULL: %s",
                esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "Wifi Station disabled successfully.");
        return true;
    }

protected:
    static void eventHandler(
        void* arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data)
    {
        auto* staConfig = static_cast<WifiConfigStation*>(arg);
        if (event_base == WIFI_EVENT)
        {
            if (event_id == WIFI_EVENT_STA_START)
            {
                esp_wifi_connect();
            }
            else if (event_id == WIFI_EVENT_STA_CONNECTED)
            {
                ESP_LOGI(TAG, "Station connected to AP");
            }
            else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
            {
                auto* event
                    = static_cast<wifi_event_sta_disconnected_t*>(event_data);
                ESP_LOGI(TAG, "Station disconnected from AP");
                if (staConfig->onDisconnected_)
                    staConfig->onDisconnected_(event->reason);
            }
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            auto* event = static_cast<ip_event_got_ip_t*>(event_data);
            ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
            if (staConfig->onConnected_)
            {
                std::string ip
                    = std::format("{}.{}.{}.{}", IP2STR(&event->ip_info.ip));
                staConfig->onConnected_(ip);
            }
        }
    }

protected:
    OnConnected onConnected_;
    OnDisconnected onDisconnected_;

private:
    esp_event_handler_instance_t wifiEventHandlerInstance_{};
    esp_event_handler_instance_t ipEventHandlerInstance_{};
};

class WifiConfigAPandStation : public WifiConfigAP, public WifiConfigStation
{
private:
    inline static constexpr const char* TAG = "WifiConfigAPandStation";

public:
    WifiConfigAPandStation(
        std::string_view apSsid,
        std::string_view apPass,
        std::string_view staSsid,
        std::string_view staPass,
        OnDeviceConnected onAPDeviceConnected = nullptr,
        OnDeviceDisconnected onAPDeviceDisconnected = nullptr,
        OnConnected onStaConnected = nullptr,
        OnDisconnected onStaDisconnected = nullptr,
        const uint8_t apChannel = 1,
        const uint8_t numConnections = 1)
        : WifiConfig(WifiMode::APandStation)
        , WifiConfigAP(
              apSsid,
              apPass,
              onAPDeviceConnected,
              onAPDeviceDisconnected,
              apChannel,
              numConnections)
        , WifiConfigStation(staSsid, staPass, onStaConnected, onStaDisconnected)
    {
    }

    virtual bool apply() override
    {
        if (esp_event_handler_instance_register(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &WifiConfigAP::eventHandler,
                this,
                &apWifiEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register AP WiFi event handler");
            return false;
        }
        if (esp_event_handler_instance_register(
                IP_EVENT,
                ESP_EVENT_ANY_ID,
                &WifiConfigAP::eventHandler,
                this,
                &apIpEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register AP IP event handler");
            return false;
        }
        if (esp_event_handler_instance_register(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &WifiConfigStation::eventHandler,
                this,
                &staWifiEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register STA WiFi event handler");
            return false;
        }
        if (esp_event_handler_instance_register(
                IP_EVENT,
                ESP_EVENT_ANY_ID,
                &WifiConfigStation::eventHandler,
                this,
                &staIpEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register STA IP event handler");
            return false;
        }

        if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set mode to WIFI_MODE_APSTA");
            return false;
        }
        if (esp_wifi_set_config(WIFI_IF_AP, &config_) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set AP config");
            return false;
        }
        if (esp_wifi_set_config(WIFI_IF_STA, &config_) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set STA config");
            return false;
        }
        return true;
    }

    virtual bool disable() override
    {
        if (esp_event_handler_instance_unregister(
                WIFI_EVENT, ESP_EVENT_ANY_ID, apWifiEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to unregister AP WiFi event handler");
            return false;
        }
        if (esp_event_handler_instance_unregister(
                IP_EVENT, ESP_EVENT_ANY_ID, apIpEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to unregister AP IP event handler");
            return false;
        }
        if (esp_event_handler_instance_unregister(
                WIFI_EVENT, ESP_EVENT_ANY_ID, staWifiEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to unregister STA WiFi event handler");
            return false;
        }
        if (esp_event_handler_instance_unregister(
                IP_EVENT, ESP_EVENT_ANY_ID, staIpEventHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to unregister STA IP event handler");
            return false;
        }

        if (esp_wifi_set_mode(WIFI_MODE_NULL) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set mode to WIFI_MODE_NULL");
            return false;
        }
        return true;
    }

private:
    esp_event_handler_instance_t apWifiEventHandlerInstance_{};
    esp_event_handler_instance_t apIpEventHandlerInstance_{};
    esp_event_handler_instance_t staWifiEventHandlerInstance_{};
    esp_event_handler_instance_t staIpEventHandlerInstance_{};
};

class WifiConfigScanner : public WifiConfig
{
public:
    struct WifiNetwork
    {
        std::string ssid;
        int rssi;
    };

private:
    inline static constexpr const char* TAG = "WifiConfigScanner";

public:
    using OnScanDone = std::function<void(const std::vector<WifiNetwork>&)>;

    WifiConfigScanner(OnScanDone onScanDone)
        : WifiConfig(WifiMode::Scanner)
        , onScanDone_{ onScanDone }
    {
    }

    virtual bool apply() override
    {
        if (esp_event_handler_instance_register(
                WIFI_EVENT,
                WIFI_EVENT_SCAN_DONE,
                &scanDoneHandler,
                this,
                &scanDoneHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register scan done handler");
            return false;
        }

        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set mode to WIFI_MODE_STA");
            return false;
        }

        if (esp_wifi_set_config(WIFI_IF_STA, &config_) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set STA config");
            return false;
        }

        return true;
    }

    virtual bool disable() override
    {
        if (esp_event_handler_instance_unregister(
                WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scanDoneHandlerInstance_)
            != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to unregister scan done handler");
            return false;
        }
        return true;
    }

    virtual bool run() override
    {
        if (esp_wifi_scan_start(nullptr, false) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start WiFi scan");
            return false;
        }
        return true;
    }

private:
    static void scanDoneHandler(
        void* arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data)
    {
        WifiConfigScanner* config = static_cast<WifiConfigScanner*>(arg);
        xTaskCreate(
            &processScanResults,
            "ScanResultsTask",
            4096,
            config,
            tskIDLE_PRIORITY + 1,
            &config->processScanResultsHandle_);
    }

    static void processScanResults(void* arg)
    {
        WifiConfigScanner* config = static_cast<WifiConfigScanner*>(arg);
        uint16_t numNetworks = 10;
        wifi_ap_record_t apRecords[10];

        if (esp_wifi_scan_get_ap_records(&numNetworks, apRecords) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get scan results");
            vTaskDelete(nullptr);
            return;
        }

        std::vector<WifiNetwork> scanResults;
        for (int i = 0; i < numNetworks; i++)
        {
            scanResults.push_back(
                { std::string(reinterpret_cast<char*>(apRecords[i].ssid)),
                  apRecords[i].rssi });
        }

        ESP_LOGI(
            TAG, "WiFi scan completed with %d networks found.", numNetworks);

        if (config->onScanDone_)
        {
            config->onScanDone_(scanResults);
        }

        vTaskDelete(nullptr);
    }

private:
    OnScanDone onScanDone_;
    esp_event_handler_instance_t scanDoneHandlerInstance_{};
    TaskHandle_t processScanResultsHandle_ = nullptr;
};

}  // namespace EspWifiManager

#endif  // _ESP_WIFI_MANAGER_CXX_WIFI_CONFIG_HPP
