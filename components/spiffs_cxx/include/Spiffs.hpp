#ifndef SPIFFS_CXX_SPIFFS_HPP
#define SPIFFS_CXX_SPIFFS_HPP

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include <esp_spiffs.h>
#include <esp_vfs.h>

class Spiffs
{
public:
    enum class Error
    {
        None = 0,
        NotInitialized,
        AlreadyInitialized,
        MountFailed,
        UnmountFailed,
        FileOpenFailed,
        WriteFailed,
        ReadFailed,
        RemoveFailed,
        SerializeFailed,
        DeserializeFailed
    };

    struct Config
    {
        std::string_view basePath = "/spiffs";
        std::string_view partitionLabel = "";
        size_t maxFiles = 5;
        bool formatIfMountFailed = true;
    };

    Spiffs() noexcept;
    ~Spiffs() noexcept;

    std::expected<void, Error> init(const Config& cfg) noexcept;
    std::expected<void, Error> deinit() noexcept;
    bool isInitialized() const noexcept;

    std::expected<void, Error> write(
        std::string_view path, std::span<const std::byte> data) const noexcept;

    std::expected<size_t, Error>
    read(std::string_view path, std::span<std::byte> buffer) const noexcept;

    std::expected<void, Error> remove(std::string_view path) const noexcept;
    std::expected<bool, Error> exists(std::string_view path) const noexcept;

    template<typename Serializer, typename T>
    std::expected<void, Error> writeObject(
        std::string_view path,
        const T& obj,
        std::span<std::byte> buffer) const noexcept
        requires requires {
            { Serializer::maxSize } -> std::convertible_to<size_t>;
            {
                Serializer::serialize(obj, buffer)
            } -> std::convertible_to<size_t>;
        }
    {
        {
            if (buffer.size() < Serializer::maxSize)
            {
                return std::unexpected(Error::SerializeFailed);
            }
        }
        size_t len = Serializer::serialize(obj, buffer);
        {
            if (len == 0 || len > buffer.size())
            {
                return std::unexpected(Error::SerializeFailed);
            }
        }
        std::span<const std::byte> data(buffer.data(), len);
        return write(path, data);
    }

    template<typename Serializer, typename T>
    std::expected<T, Error> readObject(
        std::string_view path, std::span<std::byte> buffer) const noexcept
        requires requires {
            {
                Serializer::deserialize(buffer)
            } -> std::same_as<std::optional<typename Serializer::value_type>>;
        }
    {
        auto lenOrErr = read(path, buffer);
        {
            if (!lenOrErr)
            {
                return std::unexpected(lenOrErr.error());
            }
        }
        size_t len = *lenOrErr;
        if (auto maybeObj = Serializer::deserialize(buffer.subspan(0, len));
            maybeObj)
        {
            return *maybeObj;
        }
        return std::unexpected(Error::DeserializeFailed);
    }

private:
    Config cfg_;
    bool initialized_{ false };
};

#endif  // SPIFFS_CXX_SPIFFS_HPP
