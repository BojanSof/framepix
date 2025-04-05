#ifndef _ESP_HTTP_SERVER_CXX_FORM_PARSER_HPP
#define _ESP_HTTP_SERVER_CXX_FORM_PARSER_HPP
#include <array>
#include <cctype>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

namespace EspHttpServer
{

inline char fromHex(char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

inline void urlDecodeInPlace(char* str)
{
    char* src = str;
    char* dst = str;

    while (*src)
    {
        if (*src == '%')
        {
            if (std::isxdigit(src[1]) && std::isxdigit(src[2]))
            {
                *dst++ = (fromHex(src[1]) << 4) | fromHex(src[2]);
                src += 3;
            }
            else
            {
                *dst++ = *src++;
            }
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            ++src;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

template<size_t BufferSize = 256, size_t MaxFields = 10> class FormParser
{
public:
    explicit FormParser(std::string_view raw)
    {
        const size_t len = std::min(raw.size(), buffer_.size() - 1);
        std::memcpy(buffer_.data(), raw.data(), len);
        buffer_[len] = '\0';
        parse();
    }

    std::optional<std::string_view> get(std::string_view key) const
    {
        for (size_t i = 0; i < fieldCount_; ++i)
        {
            if (fields_[i].first == key)
            {
                return fields_[i].second;
            }
        }
        return std::nullopt;
    }

private:
    std::array<char, BufferSize> buffer_;
    std::array<std::pair<std::string_view, std::string_view>, MaxFields>
        fields_;
    size_t fieldCount_ = 0;

    void parse()
    {
        char* ptr = buffer_.data();
        while (*ptr && fieldCount_ < MaxFields)
        {
            char* keyStart = ptr;
            char* keyEnd = nullptr;
            char* valStart = nullptr;
            char* valEnd = nullptr;

            // Find '='
            while (*ptr && *ptr != '=' && *ptr != '&')
                ++ptr;
            if (*ptr == '=')
            {
                keyEnd = ptr;
                *ptr++ = '\0';
                valStart = ptr;
                while (*ptr && *ptr != '&')
                    ++ptr;
                valEnd = ptr;
            }
            else
            {
                // Malformed (no '=') or end of string
                break;
            }

            if (*ptr == '&')
            {
                *ptr++ = '\0';
            }

            urlDecodeInPlace(keyStart);
            urlDecodeInPlace(valStart);

            fields_[fieldCount_++] = {
                std::string_view{ keyStart, keyEnd },
                std::string_view{ valStart, valEnd }
            };
        }
    }
};

}  // namespace EspHttpServer
#endif  //_ESP_HTTP_SERVER_CXX_FORM_PARSER_HPP
