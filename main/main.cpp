#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "HttpServer.hpp"
#include "WifiManager.hpp"

#include "WifiProvisioningWeb.hpp"

#include "LedMatrix.hpp"

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

    /* Initialize WiFi */
    using namespace EspWifiManager;
    using namespace EspWifiProvisioningWeb;

    WifiManager manager{};
    HttpServer httpServer{};
    WifiProvisioningWeb provisioningWeb{ manager, httpServer };
    provisioningWeb.start("FramePix", "12345678");

    const uint8_t val = 30;
    WS2812Matrix<16, 16> matrix(GPIO_NUM_6);
    matrix.fill({ val, 0, 0 });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.fill({ 0, val, 0 });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.fill({ 0, 0, val });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.fill({ val, val, 0 });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.fill({ val, 0, val });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.fill({ 0, val, val });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.fill({ val, val, val });
    matrix.update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    matrix.clear();
    matrix.update();

    while (1)
    {
        /*ESP_LOGI(TAG, "Provisioning example");*/
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
