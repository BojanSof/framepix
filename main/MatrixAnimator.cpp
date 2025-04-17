#include "MatrixAnimator.hpp"
#include "LedMatrix.hpp"
#include "freertos/projdefs.h"

#include <esp_log.h>

static const char* TAG = "MatrixAnimator";

template<typename MatrixT>
MatrixAnimator<MatrixT>::MatrixAnimator(MatrixT& matrix)
    : matrix_(matrix)
{
    lock_ = xSemaphoreCreateMutex();
}

template<typename MatrixT> MatrixAnimator<MatrixT>::~MatrixAnimator()
{
    stop();
    if (lock_)
    {
        vSemaphoreDelete(lock_);
    }
}

template<typename MatrixT>
void MatrixAnimator<MatrixT>::start(
    std::vector<std::array<RGB, N>, VectorAllocator>&& frames,
    uint32_t interval)
{
    xSemaphoreTake(lock_, portMAX_DELAY);
    // Stop any existing animation
    running_ = false;
    xSemaphoreGive(lock_);
    if (taskHandle_)
    {
        // wait for previous task to exit
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Move in the new frames
    xSemaphoreTake(lock_, portMAX_DELAY);
    frames_ = std::move(frames);
    interval_ = interval;
    running_ = true;
    xSemaphoreGive(lock_);

    // Create the task
    if (!taskHandle_)
    {
        xTaskCreate(
            taskEntry,
            "animTask",
            4 * 1024,
            this,
            tskIDLE_PRIORITY + 1,
            &taskHandle_);
    }
}

template<typename MatrixT> void MatrixAnimator<MatrixT>::stop()
{
    xSemaphoreTake(lock_, portMAX_DELAY);
    running_ = false;
    xSemaphoreGive(lock_);

    if (taskHandle_)
    {
        // Let the task notice running_ == false
        vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }
}

template<typename MatrixT> void MatrixAnimator<MatrixT>::taskEntry(void* arg)
{
    auto* self = static_cast<MatrixAnimator*>(arg);
    TickType_t lastWake = xTaskGetTickCount();

    size_t frameIndex = 0;
    while (true)
    {
        // Copy out local pointers under lock
        xSemaphoreTake(self->lock_, portMAX_DELAY);
        bool run = self->running_;
        auto interval = self->interval_;
        auto& buf = self->frames_;
        xSemaphoreGive(self->lock_);

        if (!run || buf.empty())
        {
            break;
        }

        // Render this frame
        self->matrix_.setAllPixels(buf[frameIndex]);
        self->matrix_.update();

        // Next frame
        frameIndex = (frameIndex + 1) % buf.size();

        // Delay until next frame
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(interval));
    }

    ESP_LOGI(TAG, "Animation task exiting");
    vTaskDelete(nullptr);
}

// Explicit template instantiation
template class MatrixAnimator<LedMatrix>;
