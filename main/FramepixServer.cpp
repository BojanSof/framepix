#include "FramepixServer.hpp"

#include <cJSON.h>

#include <esp_log.h>
#include <esp_system.h>

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
    WifiProvisioningWeb& wifiProvisioningWeb,
    StorageManager& storageManager)
    : httpServer_{ httpServer }
    , ledMatrix_{ ledMatrix }
    , animator_{ animator }
    , wifiProvisioningWeb_{ wifiProvisioningWeb }
    , storageManager_{ storageManager }
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

                                 cJSON* root = cJSON_Parse(content.c_str());
                                 if (!root)
                                 {
                                     ESP_LOGW(TAG, "Invalid Request Content");
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "Invalid request JSON", "text/plain");
                                     return response;
                                 }

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

                                 using RGB = LedMatrix::RGB;
                                 constexpr size_t N = LedMatrix::numPixels;
                                 std::vector<
                                     std::array<RGB, N>,
                                     MatrixAnimator<LedMatrix>::VectorAllocator>
                                     framesVec;
                                 framesVec.reserve(
                                     cJSON_GetArraySize(framesItem));

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

                                 animator_.start(
                                     std::move(framesVec), intervalMs);

                                 response.setStatus("200 OK");
                                 response.setContent(
                                     "Animation successful", "text/plain");
                                 return response;
                             } }
    , framepixWifiChangeUri_{ "/wifi-change",
                         HTTP_POST,
                         [this](HttpRequest req) -> HttpResponse
                         {
                             HttpResponse response(req);
                             wifiProvisioningWeb_.removePreviousProvisioning();
                             esp_restart();
                             response.setStatus("200 OK");
                             response.setContent(
                                 "WiFi change initiated successfully", "text/plain");
                             return response;
                         } }
    , saveDesignUri_{ "/save-design",
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
                              response.setContent("Invalid JSON", "text/plain");
                              return response;
                          }

                          cJSON* name = cJSON_GetObjectItem(root, "name");
                          cJSON* matrix = cJSON_GetObjectItem(root, "matrix");
                          if (!cJSON_IsString(name) || !cJSON_IsArray(matrix)
                              || cJSON_GetArraySize(matrix)
                                  != LedMatrix::numPixels)
                          {
                              cJSON_Delete(root);
                              response.setStatus("400 Bad Request");
                              response.setContent(
                                  "Invalid request format", "text/plain");
                              return response;
                          }

                          StorageManager::Design design;
                          design.name = name->valuestring;
                          bool error = false;
                          for (int i = 0; i < LedMatrix::numPixels; ++i)
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
                              design.pixels[i] = { r, g, b };
                          }

                          cJSON_Delete(root);
                          if (error)
                          {
                              response.setStatus("400 Bad Request");
                              response.setContent(
                                  "Invalid color data", "text/plain");
                              return response;
                          }

                          if (storageManager_.saveDesign(design))
                          {
                              // Save as last used design
                              storageManager_.saveLastUsed(design.name, false);
                              response.setStatus("200 OK");
                              response.setContent("Design saved", "text/plain");
                          }
                          else
                          {
                              response.setStatus("500 Internal Server Error");
                              response.setContent(
                                  "Failed to save design", "text/plain");
                          }
                          return response;
                      } }
    , saveAnimationUri_{ "/save-animation",
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
                                     "Invalid JSON", "text/plain");
                                 return response;
                             }

                             cJSON* name = cJSON_GetObjectItem(root, "name");
                             cJSON* intervalMs
                                 = cJSON_GetObjectItem(root, "interval_ms");
                             cJSON* frames
                                 = cJSON_GetObjectItem(root, "frames");
                             if (!cJSON_IsString(name)
                                 || !cJSON_IsNumber(intervalMs)
                                 || !cJSON_IsArray(frames))
                             {
                                 cJSON_Delete(root);
                                 response.setStatus("400 Bad Request");
                                 response.setContent(
                                     "Invalid request format", "text/plain");
                                 return response;
                             }

                             StorageManager::Animation animation;
                             animation.name = name->valuestring;
                             animation.intervalMs = intervalMs->valueint;

                             for (int i = 0; i < cJSON_GetArraySize(frames);
                                  ++i)
                             {
                                 cJSON* frame = cJSON_GetArrayItem(frames, i);
                                 if (!cJSON_IsArray(frame)
                                     || cJSON_GetArraySize(frame)
                                         != LedMatrix::numPixels)
                                 {
                                     cJSON_Delete(root);
                                     response.setStatus("400 Bad Request");
                                     response.setContent(
                                         "Invalid frame data", "text/plain");
                                     return response;
                                 }

                                 std::
                                     array<LedMatrix::RGB, LedMatrix::numPixels>
                                         framePixels;
                                 for (int j = 0; j < LedMatrix::numPixels; ++j)
                                 {
                                     cJSON* colorItem
                                         = cJSON_GetArrayItem(frame, j);
                                     const char* hex
                                         = cJSON_GetStringValue(colorItem);
                                     if (!hex || strlen(hex) != 7
                                         || hex[0] != '#')
                                     {
                                         cJSON_Delete(root);
                                         response.setStatus("400 Bad Request");
                                         response.setContent(
                                             "Invalid color data",
                                             "text/plain");
                                         return response;
                                     }

                                     uint8_t r, g, b;
                                     sscanf(
                                         hex + 1,
                                         "%02hhx%02hhx%02hhx",
                                         &r,
                                         &g,
                                         &b);
                                     framePixels[j] = { r, g, b };
                                 }
                                 animation.frames.push_back(framePixels);
                             }

                             cJSON_Delete(root);
                             if (animation.frames.empty())
                             {
                                 response.setStatus("400 Bad Request");
                                 response.setContent(
                                     "No frames data", "text/plain");
                                 return response;
                             }

                             if (storageManager_.saveAnimation(animation))
                             {
                                 // Save as last used animation
                                 storageManager_.saveLastUsed(animation.name, true);
                                 response.setStatus("200 OK");
                                 response.setContent(
                                     "Animation saved", "text/plain");
                             }
                             else
                             {
                                 response.setStatus(
                                     "500 Internal Server Error");
                                 response.setContent(
                                     "Failed to save animation", "text/plain");
                             }
                             return response;
                         } }
    , listDesignsUri_{ "/list-designs",
                       HTTP_GET,
                       [this](HttpRequest req) -> HttpResponse
                       {
                           HttpResponse response(req);
                           auto designs = storageManager_.listDesigns();

                           cJSON* root = cJSON_CreateObject();
                           cJSON* designsArray = cJSON_CreateArray();
                           for (const auto& name: designs)
                           {
                               cJSON_AddItemToArray(
                                   designsArray,
                                   cJSON_CreateString(name.c_str()));
                           }
                           cJSON_AddItemToObject(root, "designs", designsArray);

                           char* json = cJSON_PrintUnformatted(root);
                           response.setStatus("200 OK");
                           response.setContent(json, "application/json");

                           cJSON_free(json);
                           cJSON_Delete(root);
                           return response;
                       } }
    , listAnimationsUri_{ "/list-animations",
                          HTTP_GET,
                          [this](HttpRequest req) -> HttpResponse
                          {
                              HttpResponse response(req);
                              auto animations
                                  = storageManager_.listAnimations();

                              cJSON* root = cJSON_CreateObject();
                              cJSON* animationsArray = cJSON_CreateArray();
                              for (const auto& name: animations)
                              {
                                  cJSON_AddItemToArray(
                                      animationsArray,
                                      cJSON_CreateString(name.c_str()));
                              }
                              cJSON_AddItemToObject(
                                  root, "animations", animationsArray);

                              char* json = cJSON_PrintUnformatted(root);
                              response.setStatus("200 OK");
                              response.setContent(json, "application/json");

                              cJSON_free(json);
                              cJSON_Delete(root);
                              return response;
                          } }
    , loadDesignUri_{ "/load-design",
                      HTTP_GET,
                      [this](HttpRequest req) -> HttpResponse
                      {
                          HttpResponse response(req);
                          auto name = req.getQueryParam("name");
                          if (!name)
                          {
                              response.setStatus("400 Bad Request");
                              response.setContent(
                                  "Missing name parameter", "text/plain");
                              return response;
                          }

                          auto design = storageManager_.loadDesign(
                              std::string{ name.value() });
                          if (!design)
                          {
                              response.setStatus("404 Not Found");
                              response.setContent(
                                  "Design not found", "text/plain");
                              return response;
                          }

                          cJSON* root = cJSON_CreateObject();
                          cJSON* matrix = cJSON_CreateArray();
                          for (const auto& pixel: design->pixels)
                          {
                              char hex[8];
                              snprintf(
                                  hex,
                                  sizeof(hex),
                                  "#%02x%02x%02x",
                                  pixel.r,
                                  pixel.g,
                                  pixel.b);
                              cJSON_AddItemToArray(
                                  matrix, cJSON_CreateString(hex));
                          }
                          cJSON_AddItemToObject(root, "matrix", matrix);

                          char* json = cJSON_PrintUnformatted(root);
                          response.setStatus("200 OK");
                          response.setContent(json, "application/json");

                          cJSON_free(json);
                          cJSON_Delete(root);
                          return response;
                      } }
    , loadAnimationUri_{ "/load-animation",
                         HTTP_GET,
                         [this](HttpRequest req) -> HttpResponse
                         {
                             HttpResponse response(req);
                             auto name = req.getQueryParam("name");
                             if (!name)
                             {
                                 response.setStatus("400 Bad Request");
                                 response.setContent(
                                     "Missing name parameter", "text/plain");
                                 return response;
                             }

                             auto animation = storageManager_.loadAnimation(
                                 std::string{ name.value() });
                             if (!animation)
                             {
                                 response.setStatus("404 Not Found");
                                 response.setContent(
                                     "Animation not found", "text/plain");
                                 return response;
                             }

                             cJSON* root = cJSON_CreateObject();
                             cJSON_AddNumberToObject(
                                 root, "interval_ms", animation->intervalMs);
                             cJSON* frames = cJSON_CreateArray();
                             for (const auto& frame: animation->frames)
                             {
                                 cJSON* frameArray = cJSON_CreateArray();
                                 for (const auto& pixel: frame)
                                 {
                                     char hex[8];
                                     snprintf(
                                         hex,
                                         sizeof(hex),
                                         "#%02x%02x%02x",
                                         pixel.r,
                                         pixel.g,
                                         pixel.b);
                                     cJSON_AddItemToArray(
                                         frameArray, cJSON_CreateString(hex));
                                 }
                                 cJSON_AddItemToArray(frames, frameArray);
                             }
                             cJSON_AddItemToObject(root, "frames", frames);

                             char* json = cJSON_PrintUnformatted(root);
                             response.setStatus("200 OK");
                             response.setContent(json, "application/json");

                             cJSON_free(json);
                             cJSON_Delete(root);
                             return response;
                         } }
    , deleteDesignUri_{ "/delete-design",
                        HTTP_DELETE,
                        [this](HttpRequest req) -> HttpResponse
                        {
                            HttpResponse response(req);
                            auto name = req.getQueryParam("name");
                            if (!name)
                            {
                                response.setStatus("400 Bad Request");
                                response.setContent(
                                    "Missing name parameter", "text/plain");
                                return response;
                            }

                            if (storageManager_.deleteDesign(
                                    std::string{ name.value() }))
                            {
                                response.setStatus("200 OK");
                                response.setContent(
                                    "Design deleted", "text/plain");
                            }
                            else
                            {
                                response.setStatus("404 Not Found");
                                response.setContent(
                                    "Design not found", "text/plain");
                            }
                            return response;
                        } }
    , deleteAnimationUri_{ "/delete-animation",
                           HTTP_DELETE,
                           [this](HttpRequest req) -> HttpResponse
                           {
                               HttpResponse response(req);
                               auto name = req.getQueryParam("name");
                               if (!name)
                               {
                                   response.setStatus("400 Bad Request");
                                   response.setContent(
                                       "Missing name parameter", "text/plain");
                                   return response;
                               }

                               if (storageManager_.deleteAnimation(
                                       std::string{ name.value() }))
                               {
                                   response.setStatus("200 OK");
                                   response.setContent(
                                       "Animation deleted", "text/plain");
                               }
                               else
                               {
                                   response.setStatus("404 Not Found");
                                   response.setContent(
                                       "Animation not found", "text/plain");
                               }
                               return response;
                           } }
    , clearStorageUri_{
        "/clear-storage",
        HTTP_POST,
        [this](HttpRequest req) -> HttpResponse
        {
            HttpResponse response(req);
            if (storageManager_.clearStorage())
            {
                response.setStatus("200 OK");
                response.setContent("Storage cleared", "text/plain");
            }
            else
            {
                response.setStatus("500 Internal Server Error");
                response.setContent("Failed to clear storage", "text/plain");
            }
            return response;
        }
    }
    , loadLastUsedUri_{
        "/load-last-used",
        HTTP_GET,
        [this](HttpRequest req) -> HttpResponse
        {
            HttpResponse response(req);
            auto lastUsed = storageManager_.loadLastUsed();
            if (!lastUsed)
            {
                response.setStatus("404 Not Found");
                response.setContent("No last used design/animation", "text/plain");
                return response;
            }

            const auto& [name, isAnimation] = *lastUsed;
            cJSON* root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "name", name.c_str());
            cJSON_AddBoolToObject(root, "isAnimation", isAnimation);

            char* json = cJSON_PrintUnformatted(root);
            response.setStatus("200 OK");
            response.setContent(json, "application/json");

            cJSON_free(json);
            cJSON_Delete(root);
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
    httpServer_.registerUri(framepixWifiChangeUri_);

    // Register new storage endpoints
    httpServer_.registerUri(saveDesignUri_);
    httpServer_.registerUri(saveAnimationUri_);
    httpServer_.registerUri(listDesignsUri_);
    httpServer_.registerUri(listAnimationsUri_);
    httpServer_.registerUri(loadDesignUri_);
    httpServer_.registerUri(loadAnimationUri_);
    httpServer_.registerUri(deleteDesignUri_);
    httpServer_.registerUri(deleteAnimationUri_);
    httpServer_.registerUri(clearStorageUri_);
    httpServer_.registerUri(loadLastUsedUri_);

    // Load last used design/animation
    auto lastUsed = storageManager_.loadLastUsed();
    if (lastUsed)
    {
        const auto& [name, isAnimation] = *lastUsed;
        if (isAnimation)
        {
            auto animation = storageManager_.loadAnimation(name);
            if (animation)
            {
                animator_.start(std::move(animation->frames), animation->intervalMs);
            }
        }
        else
        {
            auto design = storageManager_.loadDesign(name);
            if (design)
            {
                animator_.stop();
                ledMatrix_.setAllPixels(design->pixels);
                ledMatrix_.update();
            }
        }
    }

    ESP_LOGI(TAG, "Framepix server started");
}

void FramepixServer::stop()
{
    httpServer_.stop();
    ESP_LOGI(TAG, "Framepix server stopped");
}
