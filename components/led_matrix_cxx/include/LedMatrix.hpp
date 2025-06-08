#ifndef LED_MATRIX_HPP
#define LED_MATRIX_HPP

#include <algorithm>
#include <array>
#include <cmath>

#include <driver/rmt_tx.h>
#include <esp_log.h>

enum class WS2812MatrixRotation : uint8_t
{
    Rot0 = 0,
    Rot90 = 1,
    Rot180 = 2,
    Rot270 = 3
};

/**
 * WS2812Matrix: Template class for driving a WS2812 pixel matrix via RMT.
 * Width, Height: dimensions of the matrix.
 * Serpentine: if true, uses serpentine mapping between rows.
 */
template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine = true,
    WS2812MatrixRotation Rotation = WS2812MatrixRotation::Rot0,
    bool MirrorX = false,
    bool MirrorY = false>
class WS2812Matrix
{
private:
    inline static constexpr const char* TAG = "WS2812Matrix";

public:
    static constexpr size_t numPixels = Width * Height;

    struct RGB
    {
        uint8_t r, g, b;

        RGB scaleAndGammaCorrect() const {
            // Color correction factors
            constexpr float r_correction = 240.0f / 255.0f;
            constexpr float g_correction = 250.0f / 255.0f;
            constexpr float b_correction = 190.0f / 255.0f;

            // Apply color correction
            float rc = r * r_correction;
            float gc = g * g_correction;
            float bc = b * b_correction;

            // Gamma correction
            constexpr float gamma = 2.5f;
            rc = 255.0f * std::pow(rc / 255.0f, gamma);
            gc = 255.0f * std::pow(gc / 255.0f, gamma);
            bc = 255.0f * std::pow(bc / 255.0f, gamma);

            // Limit maximum brightness
            constexpr uint8_t maxBrightness = 70;
            float maxChannel = std::max({rc, gc, bc});
            if (maxChannel > maxBrightness) {
                float scale = maxBrightness / maxChannel;
                rc *= scale;
                gc *= scale;
                bc *= scale;
            }

            return {
                static_cast<uint8_t>(std::round(rc)),
                static_cast<uint8_t>(std::round(gc)),
                static_cast<uint8_t>(std::round(bc))
            };
        }
    };

    explicit WS2812Matrix(gpio_num_t gpio);
    ~WS2812Matrix();

    bool init();

    void setPixel(uint16_t x, uint16_t y, RGB color);
    void setAllPixels(const std::array<RGB, numPixels>& pixels);
    void fill(RGB color);
    void clear();
    bool update();

private:
    size_t index(uint16_t x, uint16_t y) const;

    gpio_num_t gpio_;
    std::array<uint8_t, numPixels * 3> pixels_;
    rmt_channel_handle_t rmtChannel_ = nullptr;
    rmt_encoder_handle_t rmtEncoder_ = nullptr;
};

// Alias for a 16×16 serpentine matrix, used in the project
using LedMatrix
    = WS2812Matrix<16, 16, true, WS2812MatrixRotation::Rot270, false, false>;

// Explicit instantiation declaration
extern template class WS2812Matrix<
    16,
    16,
    true,
    WS2812MatrixRotation::Rot270,
    false,
    false>;

#endif  // LED_MATRIX_HPP
