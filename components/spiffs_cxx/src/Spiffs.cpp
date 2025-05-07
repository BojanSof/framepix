#include "Spiffs.hpp"
#include <string>

Spiffs::Spiffs() noexcept = default;

Spiffs::~Spiffs() noexcept { [[maybe_unused]] const auto ret = deinit(); }

std::expected<void, Spiffs::Error> Spiffs::init(const Config& cfg) noexcept
{
    {
        if (initialized_)
        {
            return std::unexpected(Error::AlreadyInitialized);
        }
    }
    cfg_ = cfg;
    esp_vfs_spiffs_conf_t conf{ .base_path = cfg_.basePath.data(),
                                .partition_label = cfg_.partitionLabel.empty()
                                    ? nullptr
                                    : cfg_.partitionLabel.data(),
                                .max_files = cfg_.maxFiles,
                                .format_if_mount_failed
                                = cfg_.formatIfMountFailed };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    {
        if (ret != ESP_OK)
        {
            return std::unexpected(Error::MountFailed);
        }
    }
    initialized_ = true;
    return {};
}

std::expected<void, Spiffs::Error> Spiffs::deinit() noexcept
{
    {
        if (!initialized_)
        {
            return std::unexpected(Error::NotInitialized);
        }
    }
    esp_err_t ret = esp_vfs_spiffs_unregister(
        cfg_.partitionLabel.empty() ? nullptr : cfg_.partitionLabel.data());
    initialized_ = false;
    {
        if (ret != ESP_OK)
        {
            return std::unexpected(Error::UnmountFailed);
        }
    }
    return {};
}

bool Spiffs::isInitialized() const noexcept { return initialized_; }

std::expected<void, Spiffs::Error> Spiffs::write(
    std::string_view path, std::span<const std::byte> data) const noexcept
{
    {
        if (!initialized_)
        {
            return std::unexpected(Error::NotInitialized);
        }
    }
    std::string full = std::string(cfg_.basePath) + '/' + std::string(path);
    FILE* f = std::fopen(full.c_str(), "wb");
    {
        if (!f)
        {
            return std::unexpected(Error::FileOpenFailed);
        }
    }
    size_t written = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    {
        if (written != data.size())
        {
            return std::unexpected(Error::WriteFailed);
        }
    }
    return {};
}

std::expected<size_t, Spiffs::Error>
Spiffs::read(std::string_view path, std::span<std::byte> buffer) const noexcept
{
    {
        if (!initialized_)
        {
            return std::unexpected(Error::NotInitialized);
        }
    }
    std::string full = std::string(cfg_.basePath) + '/' + std::string(path);
    FILE* f = std::fopen(full.c_str(), "rb");
    {
        if (!f)
        {
            return std::unexpected(Error::FileOpenFailed);
        }
    }
    size_t readBytes = std::fread(buffer.data(), 1, buffer.size(), f);
    bool err = std::ferror(f);
    std::fclose(f);
    {
        if (err)
        {
            return std::unexpected(Error::ReadFailed);
        }
    }
    return readBytes;
}

std::expected<void, Spiffs::Error>
Spiffs::remove(std::string_view path) const noexcept
{
    {
        if (!initialized_)
        {
            return std::unexpected(Error::NotInitialized);
        }
    }
    std::string full = std::string(cfg_.basePath) + '/' + std::string(path);
    {
        if (::remove(full.c_str()) != 0)
        {
            return std::unexpected(Error::RemoveFailed);
        }
    }
    return {};
}

std::expected<bool, Spiffs::Error>
Spiffs::exists(std::string_view path) const noexcept
{
    {
        if (!initialized_)
        {
            return std::unexpected(Error::NotInitialized);
        }
    }
    std::string full = std::string(cfg_.basePath) + '/' + std::string(path);
    FILE* f = std::fopen(full.c_str(), "rb");
    {
        if (f)
        {
            std::fclose(f);
            return true;
        }
    }
    return false;
}
