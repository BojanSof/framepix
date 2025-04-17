#ifndef MATRIX_ANIMATOR_HPP
#define MATRIX_ANIMATOR_HPP

#include "PSRAMallocator.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <array>
#include <vector>

template<typename MatrixT> class MatrixAnimator
{
public:
    using RGB = typename MatrixT::RGB;
    static constexpr size_t N = MatrixT::numPixels;
    using VectorAllocator = PSRAMAllocator<std::array<RGB, N>>;

    MatrixAnimator(MatrixT& matrix);
    ~MatrixAnimator();

    // Starts (or restarts) animation: takes ownership of frames and fps
    void start(
        std::vector<std::array<RGB, N>, VectorAllocator>&& frames,
        uint32_t interval);
    // Stops the animation task
    void stop();

private:
    static void taskEntry(void* arg);

    MatrixT& matrix_;
    TaskHandle_t taskHandle_{ nullptr };
    SemaphoreHandle_t lock_;
    std::vector<std::array<RGB, N>, VectorAllocator> frames_;
    uint32_t interval_{ 0 };
    bool running_{ false };
};

#endif  // MATRIX_ANIMATOR_HPP
