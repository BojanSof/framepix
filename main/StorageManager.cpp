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
    return "/" + std::string(designPrefix) + name + ".bin";
}

std::string StorageManager::getAnimationFilename(const std::string& name)
{
    return "/" + std::string(animationPrefix) + name + ".bin";
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

std::vector<uint8_t> StorageManager::serializeDesign(const Design& design)
{
    std::vector<uint8_t> data(sizeof(BinaryDesign));
    BinaryDesign* binary = reinterpret_cast<BinaryDesign*>(data.data());

    binary->magic = BinaryDesign::MAGIC;
    binary->version = BinaryDesign::VERSION;
    binary->nameLength = std::min(design.name.length(), size_t(31));
    strncpy(binary->name, design.name.c_str(), 31);
    binary->name[31] = '\0';

    // Copy RGB values directly
    for (size_t i = 0; i < LedMatrix::numPixels; i++)
    {
        binary->pixels[i * 3] = design.pixels[i].r;
        binary->pixels[i * 3 + 1] = design.pixels[i].g;
        binary->pixels[i * 3 + 2] = design.pixels[i].b;
    }

    return data;
}

std::optional<StorageManager::Design>
StorageManager::deserializeDesign(const std::vector<uint8_t>& data)
{
    if (data.size() != sizeof(BinaryDesign))
    {
        ESP_LOGE(TAG, "Invalid design data size");
        return std::nullopt;
    }

    const BinaryDesign* binary
        = reinterpret_cast<const BinaryDesign*>(data.data());
    if (binary->magic != BinaryDesign::MAGIC
        || binary->version != BinaryDesign::VERSION)
    {
        ESP_LOGE(TAG, "Invalid design format");
        return std::nullopt;
    }

    Design design;
    design.name = std::string(binary->name, binary->nameLength);

    // Copy RGB values directly
    for (size_t i = 0; i < LedMatrix::numPixels; i++)
    {
        design.pixels[i] = { binary->pixels[i * 3],
                             binary->pixels[i * 3 + 1],
                             binary->pixels[i * 3 + 2] };
    }

    return design;
}

std::vector<uint8_t>
StorageManager::serializeAnimation(const Animation& animation)
{
    size_t frameDataSize = LedMatrix::numPixels * 3 * animation.frames.size();
    std::vector<uint8_t> data(sizeof(BinaryAnimation) + frameDataSize);
    BinaryAnimation* binary = reinterpret_cast<BinaryAnimation*>(data.data());

    binary->magic = BinaryAnimation::MAGIC;
    binary->version = BinaryAnimation::VERSION;
    binary->nameLength = std::min(animation.name.length(), size_t(31));
    strncpy(binary->name, animation.name.c_str(), 31);
    binary->name[31] = '\0';
    binary->intervalMs = animation.intervalMs;
    binary->numFrames = animation.frames.size();

    // Copy frame data
    uint8_t* frameData = binary->frames;
    for (const auto& frame: animation.frames)
    {
        for (size_t i = 0; i < LedMatrix::numPixels; i++)
        {
            frameData[i * 3] = frame[i].r;
            frameData[i * 3 + 1] = frame[i].g;
            frameData[i * 3 + 2] = frame[i].b;
        }
        frameData += LedMatrix::numPixels * 3;
    }

    return data;
}

std::optional<StorageManager::Animation>
StorageManager::deserializeAnimation(const std::vector<uint8_t>& data)
{
    if (data.size() < sizeof(BinaryAnimation))
    {
        ESP_LOGE(TAG, "Invalid animation data size");
        return std::nullopt;
    }

    const BinaryAnimation* binary
        = reinterpret_cast<const BinaryAnimation*>(data.data());
    if (binary->magic != BinaryAnimation::MAGIC
        || binary->version != BinaryAnimation::VERSION)
    {
        ESP_LOGE(TAG, "Invalid animation format");
        return std::nullopt;
    }

    size_t expectedSize = sizeof(BinaryAnimation)
        + (binary->numFrames * LedMatrix::numPixels * 3);
    if (data.size() != expectedSize)
    {
        ESP_LOGE(TAG, "Invalid animation data size");
        return std::nullopt;
    }

    Animation animation;
    animation.name = std::string(binary->name, binary->nameLength);
    animation.intervalMs = binary->intervalMs;

    const uint8_t* frameData = binary->frames;
    for (uint16_t f = 0; f < binary->numFrames; f++)
    {
        std::array<LedMatrix::RGB, LedMatrix::numPixels> frame;
        for (size_t i = 0; i < LedMatrix::numPixels; i++)
        {
            frame[i] = { frameData[i * 3],
                         frameData[i * 3 + 1],
                         frameData[i * 3 + 2] };
        }
        animation.frames.push_back(frame);
        frameData += LedMatrix::numPixels * 3;
    }

    return animation;
}

bool StorageManager::writeBinaryToFile(
    const std::string& filename, const std::vector<uint8_t>& data)
{
    ESP_LOGI(TAG, "Writing binary data to file: %s", filename.c_str());
    std::span<const std::byte> dataSpan{
        reinterpret_cast<const std::byte*>(data.data()), data.size()
    };
    auto result = spiffs_.write(filename, dataSpan);
    return result.has_value();
}

std::optional<std::vector<uint8_t>> StorageManager::readBinaryFromFile(
    const std::string& filename, const size_t readBufferSize)
{
    ESP_LOGI(TAG, "Reading binary data from file: %s", filename.c_str());
    std::vector<std::byte> buffer(readBufferSize);
    auto result = spiffs_.read(filename, buffer);
    if (!result)
    {
        ESP_LOGE(TAG, "Failed to read file: %s", filename.c_str());
        return std::nullopt;
    }
    return std::vector<uint8_t>(
        reinterpret_cast<uint8_t*>(buffer.data()),
        reinterpret_cast<uint8_t*>(buffer.data()) + *result);
}

bool StorageManager::saveDesign(const Design& design)
{
    ESP_LOGI(TAG, "Saving design: %s", design.name.c_str());

    auto data = serializeDesign(design);
    std::string filename = getDesignFilename(design.name);
    bool result = writeBinaryToFile(filename, data);

    if (result)
    {
        result = updateIndexFile(
            designsIndexFile, design.name, filename, data.size());
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

    auto data = readBinaryFromFile(it->second.filename, it->second.size);
    if (!data)
        return std::nullopt;

    return deserializeDesign(*data);
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

    auto data = serializeAnimation(animation);
    std::string filename = getAnimationFilename(animation.name);
    bool result = writeBinaryToFile(filename, data);

    if (result)
    {
        result = updateIndexFile(
            animationsIndexFile, animation.name, filename, data.size());
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

    auto data = readBinaryFromFile(it->second.filename, it->second.size);
    if (!data)
        return std::nullopt;

    return deserializeAnimation(*data);
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

    std::vector<std::byte> buffer(readBufferSize);
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
