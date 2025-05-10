#include "LedMatrix.hpp"
#include "led_strip_encoder.h"

#include <esp_err.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"

template<uint16_t Width, uint16_t Height, bool Serpentine>
WS2812Matrix<Width, Height, Serpentine>::WS2812Matrix(gpio_num_t gpio)
    : gpio_(gpio)
{
}

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
bool WS2812Matrix<Width, Height, Serpentine>::init()
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

    esp_err_t err = rmt_new_tx_channel(&txConfig, &rmtChannel_);
    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(err));
        return false;
    }

    led_strip_encoder_config_t encoder_config{ .resolution = resolution };
    err = rmt_new_led_strip_encoder(&encoder_config, &rmtEncoder_);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create RMT encoder: %s", esp_err_to_name(err));
        return false;
    }

    err = rmt_enable(rmtChannel_);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(err));
        return false;
    }

    return true;
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
bool WS2812Matrix<Width, Height, Serpentine>::update()
{
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags = { .eot_level = false, .queue_nonblocking = false }
    };

    esp_err_t err = rmt_transmit(
        rmtChannel_,
        rmtEncoder_,
        reinterpret_cast<const uint8_t*>(pixels_.data()),
        pixels_.size(),
        &tx_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to transmit RMT data: %s", esp_err_to_name(err));
        return false;
    }

    err = rmt_tx_wait_all_done(rmtChannel_, portMAX_DELAY);
    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Timeout or error waiting for RMT transmission to complete: %s",
            esp_err_to_name(err));
        return false;
    }

    return true;
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
