#include "LedMatrix.hpp"
#include "led_strip_encoder.h"

#include <esp_err.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::
    WS2812Matrix(gpio_num_t gpio)
    : gpio_(gpio)
{
}

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::
    ~WS2812Matrix()
{
    rmt_del_encoder(rmtEncoder_);
    if (rmtChannel_)
    {
        rmt_disable(rmtChannel_);
        rmt_del_channel(rmtChannel_);
    }
}

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
bool WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::init()
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

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
void WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::
    setPixel(uint16_t x, uint16_t y, RGB color)
{
    if (x < Width && y < Height)
    {
        auto idx = 3 * index(x, y);
        pixels_[idx + 0] = color.g;
        pixels_[idx + 1] = color.r;
        pixels_[idx + 2] = color.b;
    }
}

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
void WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::
    setAllPixels(const std::array<RGB, numPixels>& pixels)
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

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
void WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::fill(
    RGB color)
{
    for (size_t i = 0; i < numPixels; ++i)
    {
        pixels_[3 * i + 0] = color.g;
        pixels_[3 * i + 1] = color.r;
        pixels_[3 * i + 2] = color.b;
    }
}

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
void WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::
    clear()
{
    fill({ 0, 0, 0 });
}

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
bool WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::
    update()
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

template<
    uint16_t Width,
    uint16_t Height,
    bool Serpentine,
    WS2812MatrixRotation Rotation,
    bool MirrorX,
    bool MirrorY>
size_t
WS2812Matrix<Width, Height, Serpentine, Rotation, MirrorX, MirrorY>::index(
    uint16_t x, uint16_t y) const
{
    // 1) Rotate
    uint16_t tx = x, ty = y;
    if constexpr (Rotation == WS2812MatrixRotation::Rot90)
    {
        tx = Height - 1 - y;
        ty = x;
    }
    else if constexpr (Rotation == WS2812MatrixRotation::Rot180)
    {
        tx = Width - 1 - x;
        ty = Height - 1 - y;
    }
    else if constexpr (Rotation == WS2812MatrixRotation::Rot270)
    {
        tx = y;
        ty = Width - 1 - x;
    }

    // 2) Mirror
    if constexpr (MirrorX)
    {
        // after 90/270 swap dims, but compile-time math still works:
        tx
            = ((Rotation == WS2812MatrixRotation::Rot90
                || Rotation == WS2812MatrixRotation::Rot270)
                   ? Height - 1 - tx
                   : Width - 1 - tx);
    }
    if constexpr (MirrorY)
    {
        ty
            = ((Rotation == WS2812MatrixRotation::Rot90
                || Rotation == WS2812MatrixRotation::Rot270)
                   ? Width - 1 - ty
                   : Height - 1 - ty);
    }

    // 3) Serpentine / straight index
    //    we invert y so row-0 is bottom of panel
    constexpr uint16_t W = Width;
    constexpr uint16_t H = Height;
    uint16_t row = (H - 1) - ty;

    if constexpr (Serpentine)
    {
        if (row & 1)
        {
            // odd row ⇒ left-to-right reversed
            return row * W + (W - 1 - tx);
        }
        else
        {
            // even row ⇒ normal
            return row * W + tx;
        }
    }
    else
    {
        return row * W + tx;
    }
}

// Explicit instantiation for the 16×16 serpentine matrix
template class WS2812Matrix<
    16,
    16,
    true,
    WS2812MatrixRotation::Rot270,
    false,
    false>;
