#ifndef FRAMEPIX_SERVER_HPP
#define FRAMEPIX_SERVER_HPP

#include "LedMatrix.hpp"
#include "MatrixAnimator.hpp"
#include <HttpServer.hpp>

using namespace EspHttpServer;

class FramepixServer
{
    inline static constexpr const char* TAG = "FramepixServer";

public:
    FramepixServer(
        HttpServer& httpServer,
        LedMatrix& ledMatrix,
        MatrixAnimator<LedMatrix>& animator);
    void start();
    void stop();

private:
    HttpServer& httpServer_;
    LedMatrix& ledMatrix_;
    MatrixAnimator<LedMatrix>& animator_;
    HttpUri framepixPageUri_;
    HttpUri framepixCssUri_;
    HttpUri framepixJsUri_;
    HttpUri framepixMatrixUri_;
    HttpUri framepixAnimationUri_;
};

#endif  // FRAMEPIX_SERVER_HPP
