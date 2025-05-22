#include "StorageManager.hpp"

#include <esp_log.h>

#include <cJSON.h>

#include <cstring>
#include <span>

StorageManager::StorageManager(Spiffs& spiffs)
    : spiffs_(spiffs)
{
}

bool StorageManager::init()
{
    ESP_LOGI(TAG, "Initializing storage");

    // Initialize index files
    if (!initIndexFile(designsIndexFile))
    {
        ESP_LOGE(TAG, "Failed to initialize designs index file");
        return false;
    }

    if (!initIndexFile(animationsIndexFile))
    {
        ESP_LOGE(TAG, "Failed to initialize animations index file");
        return false;
    }

    return true;
}

bool StorageManager::initIndexFile(const std::string& filename)
{
    ESP_LOGI(TAG, "Initializing index file: %s", filename.c_str());

    if (spiffs_.exists(filename).value_or(false))
    {
        // Verify the file is valid JSON
        auto json = readJsonFromFile(filename);
        if (!json)
        {
            ESP_LOGI(
                TAG,
                "Index file exists but is invalid, recreating: %s",
                filename.c_str());
        }
        else
        {
            return true;
        }
    }

    // Create new index file with empty entries object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddObjectToObject(root, "entries");
    char* json = cJSON_PrintUnformatted(root);
    std::span<const std::byte> data{ reinterpret_cast<const std::byte*>(json),
                                     strlen(json) };
    auto writeRes = spiffs_.write(filename, data);
    cJSON_free(json);
    cJSON_Delete(root);

    return writeRes.has_value();
}

std::string StorageManager::getDesignFilename(const std::string& name)
{
    return "/" + std::string(designPrefix) + name + ".json";
}

std::string StorageManager::getAnimationFilename(const std::string& name)
{
    return "/" + std::string(animationPrefix) + name + ".json";
}

bool StorageManager::updateIndexFile(
    const std::string& indexFile,
    const std::string& name,
    const std::string& filename,
    size_t size)
{
    auto entries = readIndexFile(indexFile);
    entries[name] = { filename, size };

    cJSON* root = cJSON_CreateObject();
    cJSON* entriesObj = cJSON_CreateObject();
    for (const auto& [entryName, entry]: entries)
    {
        cJSON* entryObj = cJSON_CreateObject();
        cJSON_AddStringToObject(entryObj, "filename", entry.filename.c_str());
        cJSON_AddNumberToObject(entryObj, "size", entry.size);
        cJSON_AddItemToObject(entriesObj, entryName.c_str(), entryObj);
    }
    cJSON_AddItemToObject(root, "entries", entriesObj);

    char* json = cJSON_PrintUnformatted(root);
    bool result = writeJsonToFile(indexFile, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return result;
}

bool StorageManager::removeFromIndexFile(
    const std::string& indexFile, const std::string& name)
{
    auto entries = readIndexFile(indexFile);
    entries.erase(name);

    cJSON* root = cJSON_CreateObject();
    cJSON* entriesObj = cJSON_CreateObject();
    for (const auto& [entryName, entry]: entries)
    {
        cJSON* entryObj = cJSON_CreateObject();
        cJSON_AddStringToObject(entryObj, "filename", entry.filename.c_str());
        cJSON_AddNumberToObject(entryObj, "size", entry.size);
        cJSON_AddItemToObject(entriesObj, entryName.c_str(), entryObj);
    }
    cJSON_AddItemToObject(root, "entries", entriesObj);

    char* json = cJSON_PrintUnformatted(root);
    bool result = writeJsonToFile(indexFile, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return result;
}

std::map<std::string, StorageManager::StorageEntry>
StorageManager::readIndexFile(const std::string& filename)
{
    std::map<std::string, StorageEntry> entries;
    auto json = readJsonFromFile(filename);
    if (!json)
        return entries;

    cJSON* root = cJSON_Parse(json->c_str());
    if (!root)
        return entries;

    cJSON* entriesObj = cJSON_GetObjectItem(root, "entries");
    if (cJSON_IsObject(entriesObj))
    {
        cJSON* entry = nullptr;
        cJSON_ArrayForEach(entry, entriesObj)
        {
            if (!cJSON_IsObject(entry))
                continue;

            cJSON* filename = cJSON_GetObjectItem(entry, "filename");
            cJSON* size = cJSON_GetObjectItem(entry, "size");
            if (cJSON_IsString(filename) && cJSON_IsNumber(size))
            {
                entries[entry->string]
                    = { filename->valuestring,
                        static_cast<size_t>(size->valueint) };
            }
        }
    }

    cJSON_Delete(root);
    return entries;
}

bool StorageManager::saveDesign(const Design& design)
{
    ESP_LOGI(TAG, "Saving design: %s", design.name.c_str());

    // Create design JSON
    cJSON* root = cJSON_CreateObject();
    cJSON* pixels = cJSON_CreateArray();
    for (const auto& pixel: design.pixels)
    {
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x", pixel.r, pixel.g, pixel.b);
        cJSON_AddItemToArray(pixels, cJSON_CreateString(hex));
    }
    cJSON_AddItemToObject(root, "pixels", pixels);

    char* json = cJSON_PrintUnformatted(root);
    std::string filename = getDesignFilename(design.name);
    bool result = writeJsonToFile(filename, json);
    cJSON_free(json);
    cJSON_Delete(root);

    if (result)
    {
        // Update index file
        result = updateIndexFile(
            designsIndexFile, design.name, filename, strlen(json));
    }

    return result;
}

std::optional<StorageManager::Design>
StorageManager::loadDesign(const std::string& name)
{
    ESP_LOGI(TAG, "Loading design: %s", name.c_str());

    auto entries = readIndexFile(designsIndexFile);
    auto it = entries.find(name);
    if (it == entries.end())
        return std::nullopt;

    auto json = readJsonFromFile(it->second.filename, it->second.size);
    if (!json)
        return std::nullopt;

    cJSON* root = cJSON_Parse(json->c_str());
    if (!root)
        return std::nullopt;

    cJSON* pixels = cJSON_GetObjectItem(root, "pixels");
    if (!cJSON_IsArray(pixels)
        || cJSON_GetArraySize(pixels) != LedMatrix::numPixels)
    {
        cJSON_Delete(root);
        return std::nullopt;
    }

    Design result;
    result.name = name;
    for (int i = 0; i < LedMatrix::numPixels; i++)
    {
        cJSON* pixel = cJSON_GetArrayItem(pixels, i);
        if (!cJSON_IsString(pixel))
        {
            cJSON_Delete(root);
            return std::nullopt;
        }
        const char* hex = pixel->valuestring;
        if (strlen(hex) != 7 || hex[0] != '#')
        {
            cJSON_Delete(root);
            return std::nullopt;
        }
        uint8_t r, g, b;
        sscanf(hex + 1, "%02hhx%02hhx%02hhx", &r, &g, &b);
        result.pixels[i] = { r, g, b };
    }

    cJSON_Delete(root);
    return result;
}

bool StorageManager::deleteDesign(const std::string& name)
{
    ESP_LOGI(TAG, "Deleting design: %s", name.c_str());

    auto entries = readIndexFile(designsIndexFile);
    auto it = entries.find(name);
    if (it == entries.end())
        return false;

    // Delete the design file
    if (!spiffs_.remove(it->second.filename).has_value())
    {
        ESP_LOGE(
            TAG,
            "Failed to delete design file: %s",
            it->second.filename.c_str());
        return false;
    }

    // Update index file
    return removeFromIndexFile(designsIndexFile, name);
}

std::vector<std::string> StorageManager::listDesigns()
{
    ESP_LOGI(TAG, "Listing designs");
    std::vector<std::string> result;

    auto entries = readIndexFile(designsIndexFile);
    for (const auto& [name, _]: entries)
    {
        result.push_back(name);
    }

    return result;
}

bool StorageManager::saveAnimation(const Animation& animation)
{
    ESP_LOGI(TAG, "Saving animation: %s", animation.name.c_str());

    // Create animation JSON
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interval_ms", animation.intervalMs);

    cJSON* frames = cJSON_CreateArray();
    for (const auto& frame: animation.frames)
    {
        cJSON* frameArray = cJSON_CreateArray();
        for (const auto& pixel: frame)
        {
            char hex[8];
            snprintf(
                hex, sizeof(hex), "#%02x%02x%02x", pixel.r, pixel.g, pixel.b);
            cJSON_AddItemToArray(frameArray, cJSON_CreateString(hex));
        }
        cJSON_AddItemToArray(frames, frameArray);
    }
    cJSON_AddItemToObject(root, "frames", frames);

    char* json = cJSON_PrintUnformatted(root);
    std::string filename = getAnimationFilename(animation.name);
    bool result = writeJsonToFile(filename, json);
    cJSON_free(json);
    cJSON_Delete(root);

    if (result)
    {
        // Update index file
        result = updateIndexFile(
            animationsIndexFile, animation.name, filename, strlen(json));
    }

    return result;
}

std::optional<StorageManager::Animation>
StorageManager::loadAnimation(const std::string& name)
{
    ESP_LOGI(TAG, "Loading animation: %s", name.c_str());

    auto entries = readIndexFile(animationsIndexFile);
    auto it = entries.find(name);
    if (it == entries.end())
        return std::nullopt;

    auto json = readJsonFromFile(it->second.filename, it->second.size);
    if (!json)
        return std::nullopt;

    cJSON* root = cJSON_Parse(json->c_str());
    if (!root)
        return std::nullopt;

    cJSON* intervalMs = cJSON_GetObjectItem(root, "interval_ms");
    cJSON* frames = cJSON_GetObjectItem(root, "frames");
    if (!cJSON_IsNumber(intervalMs) || !cJSON_IsArray(frames))
    {
        cJSON_Delete(root);
        return std::nullopt;
    }

    Animation result;
    result.name = name;
    result.intervalMs = intervalMs->valueint;

    for (int i = 0; i < cJSON_GetArraySize(frames); i++)
    {
        cJSON* frame = cJSON_GetArrayItem(frames, i);
        if (!cJSON_IsArray(frame)
            || cJSON_GetArraySize(frame) != LedMatrix::numPixels)
        {
            cJSON_Delete(root);
            return std::nullopt;
        }

        std::array<LedMatrix::RGB, LedMatrix::numPixels> framePixels;
        for (int j = 0; j < LedMatrix::numPixels; j++)
        {
            cJSON* pixel = cJSON_GetArrayItem(frame, j);
            if (!cJSON_IsString(pixel))
            {
                cJSON_Delete(root);
                return std::nullopt;
            }
            const char* hex = pixel->valuestring;
            if (strlen(hex) != 7 || hex[0] != '#')
            {
                cJSON_Delete(root);
                return std::nullopt;
            }
            uint8_t r, g, b;
            sscanf(hex + 1, "%02hhx%02hhx%02hhx", &r, &g, &b);
            framePixels[j] = { r, g, b };
        }
        result.frames.push_back(framePixels);
    }

    cJSON_Delete(root);
    return result;
}

bool StorageManager::deleteAnimation(const std::string& name)
{
    ESP_LOGI(TAG, "Deleting animation: %s", name.c_str());

    auto entries = readIndexFile(animationsIndexFile);
    auto it = entries.find(name);
    if (it == entries.end())
        return false;

    // Delete the animation file
    if (!spiffs_.remove(it->second.filename).has_value())
    {
        ESP_LOGE(
            TAG,
            "Failed to delete animation file: %s",
            it->second.filename.c_str());
        return false;
    }

    // Update index file
    return removeFromIndexFile(animationsIndexFile, name);
}

std::vector<std::string> StorageManager::listAnimations()
{
    ESP_LOGI(TAG, "Listing animations");
    std::vector<std::string> result;

    auto entries = readIndexFile(animationsIndexFile);
    for (const auto& [name, _]: entries)
    {
        result.push_back(name);
    }

    return result;
}

bool StorageManager::clearStorage()
{
    ESP_LOGI(TAG, "Clearing storage");

    // Delete all files in designs directory
    auto designs = listDesigns();
    for (const auto& name: designs)
    {
        if (!deleteDesign(name))
        {
            ESP_LOGE(TAG, "Failed to delete design: %s", name.c_str());
            return false;
        }
    }

    // Delete all files in animations directory
    auto animations = listAnimations();
    for (const auto& name: animations)
    {
        if (!deleteAnimation(name))
        {
            ESP_LOGE(TAG, "Failed to delete animation: %s", name.c_str());
            return false;
        }
    }

    // Reinitialize storage
    return init();
}

bool StorageManager::writeJsonToFile(
    const std::string& filename, const std::string& json)
{
    ESP_LOGI(TAG, "Writing JSON to file: %s", filename.c_str());
    std::span<const std::byte> data{
        reinterpret_cast<const std::byte*>(json.data()), json.size()
    };
    auto result = spiffs_.write(filename, data);
    return result.has_value();
}

std::optional<std::string> StorageManager::readJsonFromFile(
    const std::string& filename, const size_t readBufferSize)
{
    ESP_LOGI(TAG, "Reading JSON from file: %s", filename.c_str());

    std::vector<std::byte> buffer(10240);
    auto result = spiffs_.read(filename, buffer);
    if (!result)
    {
        ESP_LOGE(TAG, "Failed to read file: %s", filename.c_str());
        return std::nullopt;
    }

    std::string content(reinterpret_cast<char*>(buffer.data()), *result);
    ESP_LOGI(TAG, "File content: %s", content.c_str());

    cJSON* root = cJSON_Parse(content.c_str());
    if (!root)
    {
        ESP_LOGE(TAG, "Invalid JSON in file: %s", filename.c_str());
        return std::nullopt;
    }

    cJSON_Delete(root);
    return content;
}
