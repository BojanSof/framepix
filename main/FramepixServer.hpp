#ifndef FRAMEPIX_SERVER_HPP
#define FRAMEPIX_SERVER_HPP

#include "LedMatrix.hpp"
#include <HttpServer.hpp>

using namespace EspHttpServer;

class FramepixServer
{
    inline static constexpr const char* TAG = "FramepixServer";

public:
    FramepixServer(HttpServer& httpServer, LedMatrix& ledMatrix);
    void start();
    void stop();

private:
    HttpServer& httpServer_;
    LedMatrix& ledMatrix_;
    HttpUri framepixPageUri_;
    HttpUri framepixCssUri_;
    HttpUri framepixJsUri_;
    HttpUri framepixMatrixUri_;
};

#endif  // FRAMEPIX_SERVER_HPP
