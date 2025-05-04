#include "LedMatrix.hpp"
#include <esp_err.h>

// Constructor
template<uint16_t Width, uint16_t Height, bool Serpentine>
WS2812Matrix<Width, Height, Serpentine>::WS2812Matrix(gpio_num_t gpio)
    : gpio_(gpio)
{
    static constexpr uint32_t resolution = 10'000'000;  // 10 MHz
    rmt_tx_channel_config_t txConfig = {
        .gpio_num = gpio_,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .intr_priority = 0,
        .flags = { .invert_out = false,
                  .with_dma = false,
                  .io_loop_back = false,
                  .io_od_mode = false,
                  .allow_pd = false },
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&txConfig, &rmtChannel_));

    led_strip_encoder_config_t encoder_config{ .resolution = resolution };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &rmtEncoder_));
    ESP_ERROR_CHECK(rmt_enable(rmtChannel_));
}

// Destructor
template<uint16_t Width, uint16_t Height, bool Serpentine>
WS2812Matrix<Width, Height, Serpentine>::~WS2812Matrix()
{
    rmt_del_encoder(rmtEncoder_);
    if (rmtChannel_)
    {
        rmt_disable(rmtChannel_);
        rmt_del_channel(rmtChannel_);
    }
}

template<uint16_t Width, uint16_t Height, bool Serpentine>
void WS2812Matrix<Width, Height, Serpentine>::setPixel(
    uint16_t x, uint16_t y, RGB color)
{
    if (x < Width && y < Height)
    {
        auto idx = 3 * index(x, y);
        pixels_[idx + 0] = color.g;
        pixels_[idx + 1] = color.r;
        pixels_[idx + 2] = color.b;
    }
}

template<uint16_t Width, uint16_t Height, bool Serpentine>
void WS2812Matrix<Width, Height, Serpentine>::setAllPixels(
    const std::array<RGB, numPixels>& pixels)
{
    for (uint16_t y = 0; y < Height; ++y)
    {
        for (uint16_t x = 0; x < Width; ++x)
        {
            size_t li = y * Width + x;
            size_t idx = 3 * index(x, y);
            pixels_[idx + 0] = pixels[li].g;
            pixels_[idx + 1] = pixels[li].r;
            pixels_[idx + 2] = pixels[li].b;
        }
    }
}

template<uint16_t Width, uint16_t Height, bool Serpentine>
void WS2812Matrix<Width, Height, Serpentine>::fill(RGB color)
{
    for (size_t i = 0; i < numPixels; ++i)
    {
        pixels_[3 * i + 0] = color.g;
        pixels_[3 * i + 1] = color.r;
        pixels_[3 * i + 2] = color.b;
    }
}

template<uint16_t Width, uint16_t Height, bool Serpentine>
void WS2812Matrix<Width, Height, Serpentine>::clear()
{
    fill({ 0, 0, 0 });
}

template<uint16_t Width, uint16_t Height, bool Serpentine>
void WS2812Matrix<Width, Height, Serpentine>::update()
{
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags = { .eot_level = false, .queue_nonblocking = false }
    };
    ESP_ERROR_CHECK(rmt_transmit(
        rmtChannel_,
        rmtEncoder_,
        reinterpret_cast<const uint8_t*>(pixels_.data()),
        pixels_.size(),
        &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(rmtChannel_, portMAX_DELAY));
}

template<uint16_t Width, uint16_t Height, bool Serpentine>
size_t
WS2812Matrix<Width, Height, Serpentine>::index(uint16_t x, uint16_t y) const
{
    y = Height - 1 - y;
    if constexpr (Serpentine)
    {
        return (y % 2 == 0) ? (y * Width + x) : (y * Width + (Width - 1 - x));
    }
    else
    {
        return y * Width + x;
    }
}

// Explicit instantiation for the 16×16 serpentine matrix
template class WS2812Matrix<16, 16, true>;
