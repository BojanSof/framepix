#ifndef FRAMEPIX_SERVER_HPP
#define FRAMEPIX_SERVER_HPP

#include "HttpServer.hpp"
#include "LedMatrix.hpp"
#include "MatrixAnimator.hpp"
#include "StorageManager.hpp"
#include "WifiProvisioningWeb.hpp"

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
        WifiProvisioningWeb& wifiProvisioningWeb,
        StorageManager& storageManager);
    void start();
    void stop();

private:
    HttpServer& httpServer_;
    LedMatrix& ledMatrix_;
    MatrixAnimator<LedMatrix>& animator_;
    WifiProvisioningWeb& wifiProvisioningWeb_;
    StorageManager& storageManager_;
    HttpUri framepixPageUri_;
    HttpUri framepixCssUri_;
    HttpUri framepixJsUri_;
    HttpUri jszipUri_;
    HttpUri framepixMatrixUri_;
    HttpUri framepixAnimationUri_;
    HttpUri framepixResetUri_;

    HttpUri saveDesignUri_;
    HttpUri saveAnimationUri_;
    HttpUri listDesignsUri_;
    HttpUri listAnimationsUri_;
    HttpUri loadDesignUri_;
    HttpUri loadAnimationUri_;
    HttpUri deleteDesignUri_;
    HttpUri deleteAnimationUri_;
    HttpUri clearStorageUri_;
};

#endif  // FRAMEPIX_SERVER_HPP
