#ifndef LED_MATRIX_HPP
#define LED_MATRIX_HPP

#include "led_strip_encoder.h"

#include <driver/rmt_tx.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "portmacro.h"

#include <array>

template<uint16_t Width, uint16_t Height, bool Serpentine = true>
class WS2812Matrix
{
private:
    static constexpr size_t numPixels = Width * Height;

public:
    struct RGB
    {
        uint8_t r, g, b;
    };

    explicit WS2812Matrix(gpio_num_t gpio)
        : gpio_(gpio)
    {
        static constexpr uint32_t resolution = 10'000'000;  // 10 MHz
        rmt_tx_channel_config_t txConfig = {
            .gpio_num = gpio_,
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = resolution, // 10 MHz
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

        led_strip_encoder_config_t encoder_config
            = { .resolution = resolution };
        ESP_ERROR_CHECK(
            rmt_new_led_strip_encoder(&encoder_config, &rmtEncoder_));

        ESP_ERROR_CHECK(rmt_enable(rmtChannel_));
    }

    ~WS2812Matrix()
    {
        rmt_del_encoder(rmtEncoder_);
        if (rmtChannel_)
        {
            rmt_disable(rmtChannel_);
            rmt_del_channel(rmtChannel_);
        }
    }

    void setPixel(uint16_t x, uint16_t y, RGB color)
    {
        if (x < Width && y < Height)
        {
            pixels_[3 * index(x, y) + 0] = color.g;
            pixels_[3 * index(x, y) + 1] = color.r;
            pixels_[3 * index(x, y) + 2] = color.b;
        }
    }

    void setAllPixels(const std::array<RGB, numPixels>& pixels)
    {
        for (uint16_t y = 0; y < Height; ++y)
        {
            for (uint16_t x = 0; x < Width; ++x)
            {
                size_t linearIndex = y * Width + x;
                pixels_[3 * index(x, y) + 0] = pixels[linearIndex].g;
                pixels_[3 * index(x, y) + 1] = pixels[linearIndex].r;
                pixels_[3 * index(x, y) + 2] = pixels[linearIndex].b;
            }
        }
    }

    void fill(RGB color)
    {
        for (uint16_t y = 0; y < Height; ++y)
        {
            for (uint16_t x = 0; x < Width; ++x)
            {
                pixels_[3 * index(x, y) + 0] = color.g;
                pixels_[3 * index(x, y) + 1] = color.r;
                pixels_[3 * index(x, y) + 2] = color.b;
            }
        }
    }

    void clear() { fill({ 0, 0, 0 }); }

    void update()
    {
        rmt_transmit_config_t txConfig = {
            .loop_count = 0,
            .flags = { .eot_level = false, .queue_nonblocking = false }
        };

        ESP_ERROR_CHECK(rmt_transmit(
            rmtChannel_,
            rmtEncoder_,
            reinterpret_cast<const uint8_t*>(pixels_.data()),
            pixels_.size(),
            &txConfig));

        ESP_ERROR_CHECK(rmt_tx_wait_all_done(rmtChannel_, portMAX_DELAY));
    }

private:
    gpio_num_t gpio_;
    std::array<uint8_t, numPixels * 3> pixels_;
    rmt_channel_handle_t rmtChannel_ = nullptr;
    rmt_encoder_handle_t rmtEncoder_ = nullptr;

    size_t index(uint16_t x, uint16_t y) const
    {
        if constexpr (Serpentine)
        {
            return (y % 2 == 0) ? y * Width + x : y * Width + (Width - 1 - x);
        }
        else
        {
            return y * Width + x;
        }
    }
};

#endif  // LED_MATRIX_HPP
