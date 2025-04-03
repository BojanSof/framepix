#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "HttpServer.hpp"
#include "WifiConfig.hpp"
#include "WifiManager.hpp"

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

    WifiManager manager{ std::make_unique<WifiConfigScanner>(WifiConfigScanner{
        [](const auto& networks)
        {
            for (const auto& network: networks)
                ESP_LOGI(
                    TAG, "Found network with SSID: %s", network.ssid.c_str());
        } }) };
    /*WifiManager manager{std::make_unique<WifiConfigAP>(WifiConfigAP{
     * "ESP_CXX", "12345678"
     * })};*/
    /**/
    /*using namespace EspHttpServer;*/
    /*HttpServer httpServer;*/
    /*HttpUri uri(*/
    /*    "/",*/
    /*    HTTP_GET,*/
    /*    [](HttpRequest req) -> HttpResponse*/
    /*    {*/
    /*        HttpResponse resp{ req };*/
    /*        resp.setBody("Dummy response!", "text/plain");*/
    /*        return resp;*/
    /*    });*/
    /*ESP_ERROR_CHECK(httpServer.start());*/
    /*ESP_ERROR_CHECK(httpServer.registerUri(uri));*/
    /**/
    /*ESP_LOGI(TAG, "HTTP server started and register / URI");*/

    while (1)
    {
        /*ESP_LOGI(TAG, "Provisioning example");*/
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
