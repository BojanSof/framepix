#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <mdns.h>

#include "HttpServer.hpp"
#include "WifiManager.hpp"

#include "Spiffs.hpp"

#include "FramepixServer.hpp"
#include "LedMatrix.hpp"
#include "MatrixAnimator.hpp"
#include "StorageManager.hpp"

#include "WifiProvisioningWeb.hpp"

#define TAG "framepix"

extern "C" void app_main()
{
    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES
        || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize SPIFFS */
    Spiffs spiffs{};
    if (auto res = spiffs.init(Spiffs::Config{}); !res)
    {
        ESP_LOGE(
            TAG,
            "Failed to initialize SPIFFS, error: %d",
            static_cast<int>(res.error()));
        return;
    }

    // Initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("framepix"));
    ESP_ERROR_CHECK(mdns_instance_name_set("FramePix"));
    ESP_ERROR_CHECK(mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0));

    /* Initialize WiFi */
    using namespace EspWifiManager;
    using namespace EspWifiProvisioningWeb;

    WifiManager manager{};
    HttpServer httpServer{};
    WifiProvisioningWeb provisioningWeb{ manager, httpServer, spiffs };

    LedMatrix matrix(GPIO_NUM_6);
    if (!matrix.init())
    {
        ESP_LOGE(TAG, "Failed to initialize LED matrix");
        return;
    }
    MatrixAnimator<LedMatrix> animator{ matrix };
    StorageManager storageManager{ spiffs };
    if (!storageManager.init())
    {
        ESP_LOGE(TAG, "Failed to initialize storage manager");
        return;
    }

    FramepixServer framepixServer{
        httpServer, matrix, animator, provisioningWeb, storageManager
    };

    bool provisioningApplied = false;
    if (provisioningWeb.checkForPreviousProvisioning())
    {
        ESP_LOGI(TAG, "Found previous provisioning");
        if (provisioningWeb.applyPreviousProvisioning(
                [&framepixServer]() { framepixServer.start(); }))
        {
            ESP_LOGI(TAG, "Applying previous provisioning");
            provisioningApplied = true;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to apply previous provisioning");
        }
    }
    else
    {
        ESP_LOGI(TAG, "No previous provisioning found");
    }
    if (!provisioningApplied)
    {
        provisioningWeb.start(
            "FramePix",
            "12345678",
            [&framepixServer]() { framepixServer.start(); });
    }

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
