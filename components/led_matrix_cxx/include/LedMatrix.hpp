#ifndef LED_MATRIX_HPP
#define LED_MATRIX_HPP

#include <array>
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
