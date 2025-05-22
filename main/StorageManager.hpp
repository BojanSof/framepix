#ifndef STORAGE_MANAGER_HPP
#define STORAGE_MANAGER_HPP

#include "LedMatrix.hpp"
#include "Spiffs.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

class StorageManager
{
private:
    static constexpr const char* TAG = "StorageManager";

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
        std::vector<std::array<LedMatrix::RGB, LedMatrix::numPixels>> frames;
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

private:
    std::string getDesignFilename(const std::string& name);
    std::string getAnimationFilename(const std::string& name);
    bool initIndexFile(const std::string& filename);
    bool updateIndexFile(
        const std::string& indexFile,
        const std::string& name,
        const std::string& filename,
        size_t size);
    bool
    removeFromIndexFile(const std::string& indexFile, const std::string& name);
    std::map<std::string, StorageEntry>
    readIndexFile(const std::string& filename);

    bool writeJsonToFile(const std::string& filename, const std::string& json);
    std::optional<std::string> readJsonFromFile(
        const std::string& filename, const size_t readBufferSize = 10240);

    static constexpr const char* designsIndexFile = "/designs_index.json";
    static constexpr const char* animationsIndexFile = "/animations_index.json";
    static constexpr const char* designPrefix = "design_";
    static constexpr const char* animationPrefix = "anim_";

    Spiffs& spiffs_;
};

#endif  // STORAGE_MANAGER_HPP
