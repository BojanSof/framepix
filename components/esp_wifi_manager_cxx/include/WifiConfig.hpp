#ifndef _ESP_WIFI_MANAGER_CXX_WIFI_CONFIG_HPP
#define _ESP_WIFI_MANAGER_CXX_WIFI_CONFIG_HPP

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include <cstring>
#include <format>
#include <functional>
#include <string>

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
    virtual void apply() = 0;
    virtual void disable() = 0;

    WifiMode getMode() const { return mode_; }

    virtual void run() { }

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
        const char* ssid,
        const char* pass,
        OnDeviceConnected onDeviceConnected = nullptr,
        OnDeviceDisconnected onDeviceDisconnected = nullptr,
        const uint8_t channel = 1,
        const uint8_t numConnections = 1)
        : WifiConfig(WifiMode::AP)
        , onDeviceConnected_{ onDeviceConnected }
        , onDeviceDisconnected_{ onDeviceDisconnected }
    {
        const size_t userSsidLen = strlen(ssid);
        const size_t maxSsidLen
            = sizeof(config_.ap.ssid) / sizeof(config_.ap.ssid[0]);
        const size_t ssidLen
            = (userSsidLen < maxSsidLen) ? userSsidLen : maxSsidLen;
        strncpy(
            reinterpret_cast<char*>(config_.ap.ssid),
            reinterpret_cast<const char*>(ssid),
            ssidLen);

        config_.ap.ssid_len = ssidLen;

        const size_t userPassLen = strlen(pass);
        const size_t maxPassLen
            = sizeof(config_.ap.password) / sizeof(config_.ap.password[0]);
        const size_t passLen
            = (userPassLen < maxPassLen) ? userPassLen : maxPassLen;
        strncpy(
            reinterpret_cast<char*>(config_.ap.password),
            reinterpret_cast<const char*>(pass),
            passLen);

        config_.ap.channel = channel;

        config_.ap.max_connection = numConnections;

        config_.ap.authmode
            = (passLen == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

        config_.ap.pmf_cfg.required = true;
    }

    virtual void apply() override
    {
        // register event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &wifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &ipEventHandlerInstance_));
        // set the mode to AP and set the configuration
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config_));
    }

    virtual void disable() override
    {
        // unregister event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            IP_EVENT, ESP_EVENT_ANY_ID, &ipEventHandlerInstance_));
        // disable the AP mode by changing the mode to STA or NULL
        wifi_mode_t mode;
        ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
        if (mode == WIFI_MODE_APSTA)
        {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        }
        else
        {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
        }
    }

protected:
    static void eventHandler(
        void* arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data)
    {
        WifiConfigAP* apConfig = (WifiConfigAP*)arg;
        if (event_base == WIFI_EVENT)
        {
            if (event_id == WIFI_EVENT_AP_STACONNECTED)
            {
                wifi_event_ap_staconnected_t* event
                    = (wifi_event_ap_staconnected_t*)event_data;
                ESP_LOGI(
                    TAG,
                    "station " MACSTR " join, AID=%d",
                    MAC2STR(event->mac),
                    event->aid);
            }
            else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
            {
                wifi_event_ap_stadisconnected_t* event
                    = (wifi_event_ap_stadisconnected_t*)event_data;
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
        else if (event_base == IP_EVENT)
        {
            if (event_id == IP_EVENT_AP_STAIPASSIGNED)
            {
                ip_event_ap_staipassigned_t* event
                    = (ip_event_ap_staipassigned_t*)event_data;
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
    using OnDisconnected = std::function<void()>;

public:
    WifiConfigStation(
        const char* ssid,
        const char* pass,
        OnConnected onConnected = nullptr,
        OnDisconnected onDisconnected = nullptr)
        : WifiConfig(WifiMode::Station)
        , onConnected_{ onConnected }
        , onDisconnected_{ onDisconnected }
    {
        const size_t userSsidLen = strlen(ssid);
        const size_t maxSsidLen = sizeof(config_.sta.ssid);
        const size_t ssidLen
            = (userSsidLen < maxSsidLen) ? userSsidLen : maxSsidLen;
        strncpy(reinterpret_cast<char*>(config_.sta.ssid), ssid, ssidLen);

        const size_t userPassLen = strlen(pass);
        const size_t maxPassLen = sizeof(config_.sta.password);
        const size_t passLen
            = (userPassLen < maxPassLen) ? userPassLen : maxPassLen;
        strncpy(reinterpret_cast<char*>(config_.sta.password), pass, passLen);
    }

    virtual void apply() override
    {
        // register station event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &wifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &eventHandler,
            this,
            &ipEventHandlerInstance_));
        // set mode and configuration for station
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_));
    }

    virtual void disable() override
    {
        // unregister event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            IP_EVENT, ESP_EVENT_ANY_ID, &ipEventHandlerInstance_));
        // disable station mode
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }

protected:
    static void eventHandler(
        void* arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data)
    {
        auto* staConfig = reinterpret_cast<WifiConfigStation*>(arg);
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
                ESP_LOGI(TAG, "Station disconnected from AP");
                if (staConfig->onDisconnected_)
                {
                    staConfig->onDisconnected_();
                }
            }
        }
        else if (event_base == IP_EVENT)
        {
            if (event_id == IP_EVENT_STA_GOT_IP)
            {
                auto* event = reinterpret_cast<ip_event_got_ip_t*>(event_data);
                ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
                if (staConfig->onConnected_)
                {
                    std::string ip = std::format(
                        "{}.{}.{}.{}", IP2STR(&event->ip_info.ip));
                    staConfig->onConnected_(ip);
                }
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
    // Constructor combines the parameters for both AP and STA.
    WifiConfigAPandStation(
        const char* apSsid,
        const char* apPass,
        const char* staSsid,
        const char* staPass,
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

    virtual void apply() override
    {
        // register AP events
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &WifiConfigAP::eventHandler,
            this,
            &apWifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &WifiConfigAP::eventHandler,
            this,
            &apIpEventHandlerInstance_));
        // register STA events
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &WifiConfigStation::eventHandler,
            this,
            &staWifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &WifiConfigStation::eventHandler,
            this,
            &staIpEventHandlerInstance_));

        // set mode to APSTA
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        // set configuration for both interfaces
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config_));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_));
    }

    virtual void disable() override
    {
        // unregister all event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &apWifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            IP_EVENT, ESP_EVENT_ANY_ID, &apIpEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &staWifiEventHandlerInstance_));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            IP_EVENT, ESP_EVENT_ANY_ID, &staIpEventHandlerInstance_));
        // set mode to NULL
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
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

    virtual void apply() override
    {
        esp_event_handler_instance_register(
            WIFI_EVENT,
            WIFI_EVENT_SCAN_DONE,
            &scanDoneHandler,
            this,
            &scanDoneHandlerInstance_);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_));
    }

    virtual void disable() override
    {
        esp_event_handler_instance_unregister(
            WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &scanDoneHandlerInstance_);
    }

    virtual void run() override
    {
        ESP_ERROR_CHECK(esp_wifi_scan_start(nullptr, false));
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
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&numNetworks, apRecords));

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
}
#endif  //_ESP_WIFI_MANAGER_CXX_WIFI_CONFIG_HPP
