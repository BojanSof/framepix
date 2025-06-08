#ifndef STORAGE_MANAGER_HPP
#define STORAGE_MANAGER_HPP

#include "LedMatrix.hpp"
#include "PSRAMallocator.hpp"
#include "Spiffs.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

class StorageManager
{
private:
    static constexpr const char* TAG = "StorageManager";
    using VectorAllocator = PSRAMAllocator<std::array<LedMatrix::RGB, LedMatrix::numPixels>>;

    // Binary format structures
    struct BinaryDesign
    {
        static constexpr uint8_t MAGIC = 0x44;  // 'D'
        static constexpr uint8_t VERSION = 1;
        uint8_t magic;
        uint8_t version;
        uint8_t nameLength;
        char name[32];  // Fixed size for name
        uint8_t pixels[LedMatrix::numPixels * 3];  // RGB values
    };

    struct BinaryAnimation
    {
        static constexpr uint8_t MAGIC = 0x41;  // 'A'
        static constexpr uint8_t VERSION = 1;
        uint8_t magic;
        uint8_t version;
        uint8_t nameLength;
        char name[32];  // Fixed size for name
        uint32_t intervalMs;
        uint16_t numFrames;
        uint8_t frames[];  // Flexible array member for frame data
    };

public:
    struct Design
    {
        std::string name;
        std::array<LedMatrix::RGB, LedMatrix::numPixels> pixels;
    };

    struct Animation
    {
        std::string name;
        int intervalMs;
        std::vector<std::array<LedMatrix::RGB, LedMatrix::numPixels>, VectorAllocator> frames;
    };

    struct StorageEntry
    {
        std::string filename;
        size_t size;
    };

    explicit StorageManager(Spiffs& spiffs);
    bool init();

    bool saveDesign(const Design& design);
    std::optional<Design> loadDesign(const std::string& name);
    bool deleteDesign(const std::string& name);
    std::vector<std::string> listDesigns();

    bool saveAnimation(const Animation& animation);
    std::optional<Animation> loadAnimation(const std::string& name);
    bool deleteAnimation(const std::string& name);
    std::vector<std::string> listAnimations();
    bool clearStorage();

    bool saveLastUsed(const std::string& name, bool isAnimation);
    std::optional<std::pair<std::string, bool>> loadLastUsed();

private:
    bool initIndexFile(const std::string& filename);
    bool writeJsonToFile(const std::string& filename, const std::string& json);
    std::optional<std::string> readJsonFromFile(
        const std::string& filename, const size_t readBufferSize = 10240);
    std::string getDesignFilename(const std::string& name);
    std::string getAnimationFilename(const std::string& name);
    bool updateIndexFile(
        const std::string& indexFile,
        const std::string& name,
        const std::string& filename,
        size_t size);
    bool
    removeFromIndexFile(const std::string& indexFile, const std::string& name);
    std::map<std::string, StorageEntry>
    readIndexFile(const std::string& filename);

    // Binary format helpers
    std::vector<uint8_t> serializeDesign(const Design& design);
    std::optional<Design> deserializeDesign(const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeAnimation(const Animation& animation);
    std::optional<Animation>
    deserializeAnimation(const std::vector<uint8_t>& data);
    bool writeBinaryToFile(
        const std::string& filename, const std::vector<uint8_t>& data);
    std::optional<std::vector<uint8_t>> readBinaryFromFile(
        const std::string& filename, const size_t readBufferSize = 10240);

    static constexpr const char* designsIndexFile = "/designs_index.json";
    static constexpr const char* animationsIndexFile = "/animations_index.json";
    static constexpr const char* designPrefix = "design_";
    static constexpr const char* animationPrefix = "anim_";
    static constexpr const char* lastUsedFile = "/last_used.json";

    Spiffs& spiffs_;
};

#endif  // STORAGE_MANAGER_HPP
