#include "FramepixServer.hpp"

#include <cJSON.h>

#include <esp_log.h>

extern const uint8_t designer_html_start[] asm("_binary_designer_html_start");
extern const uint8_t designer_html_end[] asm("_binary_designer_html_end");

extern const uint8_t css_styles_css_start[] asm("_binary_styles_css_start");
extern const uint8_t css_styles_css_end[] asm("_binary_styles_css_end");

extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

FramepixServer::FramepixServer(HttpServer& httpServer, LedMatrix& ledMatrix)
    : httpServer_{ httpServer }
    , ledMatrix_{ ledMatrix }
    , framepixPageUri_{ "/",
                        HTTP_GET,
                        [](HttpRequest req) -> HttpResponse
                        {
                            HttpResponse response(req);
                            response.setStatus("200 OK");
                            const size_t designer_html_size
                                = designer_html_end - designer_html_start;
                            response.setContent(
                                std::string_view{ reinterpret_cast<const char*>(
                                                      designer_html_start),
                                                  designer_html_size },
                                "text/html");
                            return response;
                        } }
    , framepixCssUri_{ "/css/styles.css",
                       HTTP_GET,
                       [](HttpRequest req) -> HttpResponse
                       {
                           HttpResponse response(req);
                           response.setStatus("200 OK");
                           size_t css_size
                               = css_styles_css_end - css_styles_css_start;
                           response.setContent(
                               std::string_view{ reinterpret_cast<const char*>(
                                                     css_styles_css_start),
                                                 css_size },
                               "text/css");
                           return response;
                       } }
    , framepixJsUri_{ "/script.js",
                      HTTP_GET,
                      [](HttpRequest req) -> HttpResponse
                      {
                          HttpResponse response(req);
                          response.setStatus("200 OK");
                          size_t js_size = script_js_end - script_js_start;
                          response.setContent(
                              std::string_view{ reinterpret_cast<const char*>(
                                                    script_js_start),
                                                js_size },
                              "text/css");
                          return response;
                      } }
    , framepixMatrixUri_{
        "/matrix",
        HTTP_POST,
        [this](HttpRequest req) -> HttpResponse
        {
            HttpResponse response(req);
            const auto content = req.getContent();
            if (content.empty())
            {
                response.setStatus("400 Bad Request");
            }
            else
            {
                cJSON* root = cJSON_Parse(content.c_str());
                if (!root)
                {
                    response.setStatus("400 Bad Request");
                }
                else
                {
                    cJSON* matrix = cJSON_GetObjectItem(root, "matrix");
                    if (!cJSON_IsArray(matrix))
                    {
                        cJSON_Delete(root);
                        response.setStatus("400 Bad Request");
                        ESP_LOGW(TAG, "Invalid Request Content");
                    }
                    else if (cJSON_GetArraySize(matrix) != LedMatrix::numPixels)
                    {
                        cJSON_Delete(root);
                        response.setStatus("400 Bad Request");
                        ESP_LOGW(TAG, "Invalid Request Content");
                    }
                    else
                    {
                        std::array<LedMatrix::RGB, LedMatrix::numPixels>
                            pixelData;
                        bool error = false;
                        for (int i = 0; i < 256; ++i)
                        {
                            cJSON* colorItem = cJSON_GetArrayItem(matrix, i);
                            const char* hex = cJSON_GetStringValue(colorItem);
                            if (!hex || strlen(hex) != 7 || hex[0] != '#')
                            {
                                error = true;
                                break;
                            }

                            uint8_t r, g, b;
                            sscanf(hex + 1, "%02hhx%02hhx%02hhx", &r, &g, &b);
                            pixelData[i] = { r, g, b };
                        }

                        cJSON_Delete(root);
                        if (error)
                        {
                            ESP_LOGW(TAG, "Invalid color data");
                            response.setStatus("400 Bad Request");
                        }
                        else
                        {
                            ESP_LOGI(TAG, "Setting matrix");
                            ledMatrix_.setAllPixels(pixelData);
                            ledMatrix_.update();
                            response.setStatus("200 OK");
                        }
                    }
                }
            }
            return response;
        }
    }
{
}

void FramepixServer::start()
{
    httpServer_.start();
    httpServer_.registerUri(framepixPageUri_);
    httpServer_.registerUri(framepixCssUri_);
    httpServer_.registerUri(framepixJsUri_);
    httpServer_.registerUri(framepixMatrixUri_);
    ESP_LOGI(TAG, "Framepix server started");
}

void FramepixServer::stop()
{
    httpServer_.stop();
    ESP_LOGI(TAG, "Framepix server stopped");
}
