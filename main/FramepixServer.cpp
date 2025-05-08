#include "FramepixServer.hpp"
#include "esp_system.h"

#include <cJSON.h>

#include <esp_log.h>

extern const uint8_t designer_html_start[] asm("_binary_designer_html_start");
extern const uint8_t designer_html_end[] asm("_binary_designer_html_end");

extern const uint8_t css_styles_css_start[] asm("_binary_styles_css_start");
extern const uint8_t css_styles_css_end[] asm("_binary_styles_css_end");

extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

extern const uint8_t jszip_start[] asm("_binary_jszip_min_js_start");
extern const uint8_t jszip_end[] asm("_binary_jszip_min_js_end");

FramepixServer::FramepixServer(
    HttpServer& httpServer,
    LedMatrix& ledMatrix,
    MatrixAnimator<LedMatrix>& animator,
    WifiProvisioningWeb& wifiProvisioningWeb)
    : httpServer_{ httpServer }
    , ledMatrix_{ ledMatrix }
    , animator_{ animator }
    , wifiProvisioningWeb_{ wifiProvisioningWeb }
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
    , jszipUri_{ "/jszip.min.js",
                 HTTP_GET,
                 [](HttpRequest req) -> HttpResponse
                 {
                     HttpResponse response(req);
                     response.setStatus("200 OK");
                     size_t js_size = jszip_end - jszip_start;
                     response.setContent(
                         std::string_view{
                             reinterpret_cast<const char*>(jszip_start),
                             js_size },
                         "text/css");
                     return response;
                 } }
    , framepixMatrixUri_{ "/matrix",
                          HTTP_POST,
                          [this](HttpRequest req) -> HttpResponse
                          {
                              HttpResponse response(req);
                              const auto content = req.getContent();
                              if (content.empty())
                              {
                                  response.setStatus("400 Bad Request");
                                  response.setContent(
                                      "Invalid request content", "text/plain");
                                  return response;
                              }
                              cJSON* root = cJSON_Parse(content.c_str());
                              if (!root)
                              {
                                  response.setStatus("400 Bad Request");
                                  response.setContent(
                                      "Invalid request content", "text/plain");
                                  return response;
                              }
                              cJSON* matrix
                                  = cJSON_GetObjectItem(root, "matrix");
                              if (!cJSON_IsArray(matrix))
                              {
                                  ESP_LOGW(TAG, "Invalid Request Content");
                                  cJSON_Delete(root);
                                  response.setStatus("400 Bad Request");
                                  response.setContent(
                                      "Invalid request content", "text/plain");
                                  return response;
                              }
                              if (cJSON_GetArraySize(matrix)
                                  != LedMatrix::numPixels)
                              {
                                  ESP_LOGW(TAG, "Invalid Request Content");
                                  cJSON_Delete(root);
                                  response.setStatus("400 Bad Request");
                                  response.setContent(
                                      "Invalid request content", "text/plain");
                                  return response;
                              }
                              std::array<LedMatrix::RGB, LedMatrix::numPixels>
                                  pixelData;
                              bool error = false;
                              for (int i = 0; i < 256; ++i)
                              {
                                  cJSON* colorItem
                                      = cJSON_GetArrayItem(matrix, i);
                                  const char* hex
                                      = cJSON_GetStringValue(colorItem);
                                  if (!hex || strlen(hex) != 7 || hex[0] != '#')
                                  {
                                      error = true;
                                      break;
                                  }

                                  uint8_t r, g, b;
                                  sscanf(
                                      hex + 1,
                                      "%02hhx%02hhx%02hhx",
                                      &r,
                                      &g,
                                      &b);
                                  pixelData[i] = { r, g, b };
                              }

                              cJSON_Delete(root);
                              if (error)
                              {
                                  ESP_LOGW(TAG, "Invalid color data");
                                  response.setStatus("400 Bad Request");
                                  response.setContent(
                                      "Invalid color", "text/plain");
                                  return response;
                              }
                              ESP_LOGI(TAG, "Setting matrix");
                              animator_.stop();
                              ledMatrix_.setAllPixels(pixelData);
                              ledMatrix_.update();
                              response.setStatus("200 OK");
                              response.setContent("OK", "text/plain");

                              return response;
                          } }
    , framepixAnimationUri_{ "/animation",
                             HTTP_POST,
                             [this](HttpRequest req) -> HttpResponse
                             {
                                 HttpResponse response(req);
                                 const auto content = req.getContent();
                                 if (content.empty())
                                 {
                                     ESP_LOGW(TAG, "Invalid Request Content");
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "Invalid request content",
                                         "text/plain");
                                     return response;
                                 }

                                 // 2. Parse JSON
                                 cJSON* root = cJSON_Parse(content.c_str());
                                 if (!root)
                                 {
                                     ESP_LOGW(TAG, "Invalid Request Content");
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "Invalid request JSON", "text/plain");
                                     return response;
                                 }

                                 // 3. interval_ms
                                 cJSON* intervalItem
                                     = cJSON_GetObjectItem(root, "interval_ms");
                                 if (!cJSON_IsNumber(intervalItem)
                                     || intervalItem->valueint <= 0)
                                 {
                                     ESP_LOGW(
                                         TAG,
                                         "Invalid Request Content (interval "
                                         "ms)");
                                     cJSON_Delete(root);
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "Invalid request JSON", "text/plain");
                                     return response;
                                 }
                                 int intervalMs = intervalItem->valueint;

                                 // 4. frames array
                                 cJSON* framesItem
                                     = cJSON_GetObjectItem(root, "frames");
                                 if (!cJSON_IsArray(framesItem))
                                 {
                                     ESP_LOGW(
                                         TAG,
                                         "Invalid Request Content (frames)");
                                     cJSON_Delete(root);
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "Invalid request JSON", "text/plain");
                                     return response;
                                 }

                                 // Prepare container
                                 using RGB = LedMatrix::RGB;
                                 constexpr size_t N = LedMatrix::numPixels;
                                 std::vector<
                                     std::array<RGB, N>,
                                     MatrixAnimator<LedMatrix>::VectorAllocator>
                                     framesVec;
                                 framesVec.reserve(
                                     cJSON_GetArraySize(framesItem));

                                 // 5. Iterate frames
                                 for (int i = 0;
                                      i < cJSON_GetArraySize(framesItem);
                                      ++i)
                                 {
                                     cJSON* frameArr
                                         = cJSON_GetArrayItem(framesItem, i);
                                     if (!cJSON_IsArray(frameArr)
                                         || cJSON_GetArraySize(frameArr) != N)
                                     {
                                         ESP_LOGW(
                                             TAG,
                                             "Skipping invalid frame %d",
                                             i);
                                         continue;
                                     }
                                     std::array<RGB, N> framePixels{};
                                     for (size_t j = 0; j < N; ++j)
                                     {
                                         cJSON* colItem
                                             = cJSON_GetArrayItem(frameArr, j);
                                         const char* hex
                                             = cJSON_GetStringValue(colItem);
                                         if (!hex || strlen(hex) != 7
                                             || hex[0] != '#')
                                         {
                                             // default to black
                                             framePixels[j] = { 0, 0, 0 };
                                         }
                                         else
                                         {
                                             uint8_t r, g, b;
                                             sscanf(
                                                 hex + 1,
                                                 "%02hhx%02hhx%02hhx",
                                                 &r,
                                                 &g,
                                                 &b);
                                             framePixels[j] = { r, g, b };
                                         }
                                     }
                                     framesVec.push_back(framePixels);
                                 }

                                 cJSON_Delete(root);

                                 if (framesVec.empty())
                                 {
                                     ESP_LOGW(TAG, "No frames data");
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "No frames data", "text/plain");
                                     return response;
                                 }

                                 ESP_LOGI(
                                     TAG,
                                     "Received %d frames @ %d ms interval",
                                     (int)framesVec.size(),
                                     intervalMs);

                                 // 6. Start animation
                                 animator_.start(
                                     std::move(framesVec), intervalMs);

                                 response.setStatus("200 OK");
                                 response.setContent(
                                     "Animation successful", "text/plain");
                                 return response;
                             } }
    , framepixResetUri_{
        "/reset",
        HTTP_POST,
        [&wifiProvisioningWeb](HttpRequest req) -> HttpResponse
        {
            HttpResponse response(req);
            wifiProvisioningWeb.removePreviousProvisioning();
            esp_restart();
            response.setStatus("200 OK");
            response.setContent("Reset successful", "text/plain");
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
    httpServer_.registerUri(jszipUri_);
    httpServer_.registerUri(framepixMatrixUri_);
    httpServer_.registerUri(framepixAnimationUri_);
    httpServer_.registerUri(framepixResetUri_);
    ESP_LOGI(TAG, "Framepix server started");
}

void FramepixServer::stop()
{
    httpServer_.stop();
    ESP_LOGI(TAG, "Framepix server stopped");
}
