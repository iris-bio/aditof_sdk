#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <chrono>
#include <glog/logging.h>
#include <iostream>
#include <signal.h>

using namespace aditof;

Camera *camera = nullptr;
bool loop = true;

#if defined(__linux__)
void sighandler(int sig) {
    loop = false;
    if (camera) {
        camera->stop();
    }
}

void setupsignalhandler() { signal(SIGINT, &sighandler); }
#elif defined(_WIN32)
void setupsignalhandler() {}
#elif defined(__APPLE__)
void setupsignalhandler() {}
#endif

int main(int argc, char *argv[]) {

    setupsignalhandler();

    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;

    Status status = Status::OK;

    System system;
    status = system.initialize();
    if (status != Status::OK) {
        LOG(ERROR) << "Could not initialize system!";
        return 0;
    }

    std::vector<Camera *> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        LOG(WARNING) << "No cameras found";
        return 0;
    }

    camera = cameras.front();
    status = camera->initialize();
    if (status != Status::OK) {
        LOG(ERROR) << "Could not initialize camera!";
        return 0;
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        std::cout << "no frame type avaialble!";
        return 0;
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != Status::OK) {
        LOG(ERROR) << "Could not set camera frame type!";
        return 0;
    }

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        LOG(ERROR) << "no camera modes available!";
        return 0;
    }
    status = camera->setMode(modes.front());
    if (status != Status::OK) {
        LOG(ERROR) << "Could not set camera mode!";
        return 0;
    }

    aditof::Frame frame;

    int fps = 0;
    auto startTime = std::chrono::system_clock::now();
    while (loop) {
        status = camera->requestFrame(&frame);
        if (status != Status::OK) {
            return 0;
        }

        auto endTime = std::chrono::system_clock::now();
        auto elapsedTime =
            std::chrono::duration<double>(endTime - startTime).count();
        if (elapsedTime > 1.0) {
            std::cout << "Average frames per second: " << fps + 1 << std::endl;
            fps = 0;
            startTime = endTime;
        } else {
            ++fps;
        }
    }

    return 0;
}
