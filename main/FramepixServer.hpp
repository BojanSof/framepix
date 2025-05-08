#ifndef FRAMEPIX_SERVER_HPP
#define FRAMEPIX_SERVER_HPP

#include "WifiProvisioningWeb.hpp"

#include <HttpServer.hpp>
#include <LedMatrix.hpp>
#include <MatrixAnimator.hpp>

using namespace EspHttpServer;
using namespace EspWifiProvisioningWeb;

class FramepixServer
{
    inline static constexpr const char* TAG = "FramepixServer";

public:
    FramepixServer(
        HttpServer& httpServer,
        LedMatrix& ledMatrix,
        MatrixAnimator<LedMatrix>& animator,
        WifiProvisioningWeb& wifiProvisioningWeb);
    void start();
    void stop();

private:
    HttpServer& httpServer_;
    LedMatrix& ledMatrix_;
    MatrixAnimator<LedMatrix>& animator_;
    WifiProvisioningWeb& wifiProvisioningWeb_;
    HttpUri framepixPageUri_;
    HttpUri framepixCssUri_;
    HttpUri framepixJsUri_;
    HttpUri jszipUri_;
    HttpUri framepixMatrixUri_;
    HttpUri framepixAnimationUri_;
    HttpUri framepixResetUri_;
};

#endif  // FRAMEPIX_SERVER_HPP
