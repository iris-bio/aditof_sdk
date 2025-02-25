#include "aditofdemoview.h"

#include <sstream>

#define CVUI_IMPLEMENTATION
#include "cvui.h"

//#define SHOW_ADVANCED_GUI

namespace detail {
std::string getKeyPressed(int key, bool &backspace) {
    switch (key & 0xff) {
    case 48:
        return "0";
    case 49:
        return "1";
    case 50:
        return "2";
    case 51:
        return "3";
    case 52:
        return "4";
    case 53:
        return "5";
    case 54:
        return "6";
    case 55:
        return "7";
    case 56:
        return "8";
    case 57:
        return "9";
    case 97:
        return "a";
    case 98:
        return "b";
    case 99:
        return "c";
    case 100:
        return "d";
    case 101:
        return "e";
    case 102:
        return "f";
    case 8:
        backspace = true;
        return "";

    default:
        return "";
    }
}

uint16_t fromHexToUint(const std::string &hexValue) {
    uint16_t value = 0;
    std::stringstream ss;
    ss << std::hex << hexValue;
    ss >> value;
    return value;
}

} // namespace detail

AdiTofDemoView::AdiTofDemoView(std::shared_ptr<AdiTofDemoController> &ctrl,
                               const std::string &name)
    : m_ctrl(ctrl), m_viewName(name), m_depthFrameAvailable(false),
      m_irFrameAvailable(false), m_stopWorkersFlag(false), m_center(true),
      m_waitKeyBarrier(0), m_distanceVal(0) {
    // cv::setNumThreads(2);
    m_depthImageWorker =
        std::thread(std::bind(&AdiTofDemoView::_displayDepthImage, this));
    m_irImageWorker =
        std::thread(std::bind(&AdiTofDemoView::_displayIrImage, this));
}
AdiTofDemoView::~AdiTofDemoView() {
    std::unique_lock<std::mutex> lock(m_frameCapturedMutex);
    m_stopWorkersFlag = true;
    lock.unlock();
    m_frameCapturedCv.notify_all();
    m_depthImageWorker.join();
    m_irImageWorker.join();
}

void AdiTofDemoView::render() {

    bool nearModeChecked = true;
    bool mediumModeChecked = false;
    bool farModeChecked = false;
    int modeCurrentValue = 4; // 4 = near(default); 2 = medium; 1 = far

    bool lowLevelChecked = false;
    bool mediumLevelChecked = false;
    bool highLevelChecked = false;
    int levelCurrentValue = 2; // 4 = low; 2 = mediu(default); 1 = high

    bool livePlayChecked = true;
    bool playbackChecked = false;
    int playCurrentValue = 2;

    int pulseCount = 2000;

    unsigned int normalColor = 0x000032;
    unsigned int selectedColor = 0xffffff;
    unsigned int errorColor = 0xff0000;

    std::string address = "0x";
    std::string value = "0x";
    std::string fileName = "";
    std::string status = "";

    unsigned int addressColor = normalColor;
    unsigned int valueColor = normalColor;
    unsigned int fieldColor = normalColor;

    bool addressFieldSelected = false;
    bool valueFieldSelected = false;
    bool fileNameFieldSelected = false;

    const cv::String windows[] = {m_viewName, "Depth Image", "IR Image"};
    cvui::init(windows, 1);

    int frameCount = 0;
    int displayFps = 0;
    auto startTime = std::chrono::system_clock::now();

    bool captureEnabled = false;
    bool recordEnabled = false;
    bool playbackEnabled = false;

    unsigned int frameHeight = 0;
    unsigned int frameWidth = 0;

    int numberOfFrames = 0;

    std::string modes[3] = {"near", "medium", "far"};

    char afe_temp_str[32] = "AFE TEMP:";
    char laser_temp_str[32] = "LASER TEMP:";
    int temp_cnt = 0;

    while (true) {
        // Fill the frame with a nice color
        cv::Mat frame = cv::Mat(400, 400, CV_8UC3);

        frame = cv::Scalar(49, 52, 49);

        cvui::context(m_viewName);

        // Exit button
        if (cvui::button(frame, 375, 0, 25, 25, "X")) {
            break;
        }

        bool checkboxChanged = false;

        // Mode checkbox group
        int btnGroupMode =
            nearModeChecked << 2 | mediumModeChecked << 1 | farModeChecked;
        if (modeCurrentValue != btnGroupMode) {
            int xorValue = modeCurrentValue ^ btnGroupMode;
            modeCurrentValue = xorValue;
            nearModeChecked = xorValue & (1 << 2);
            mediumModeChecked = xorValue & (1 << 1);
            farModeChecked = xorValue & 1;
            checkboxChanged = true;
        }

        // Level checkbox group
        int btnGroupLevel =
            lowLevelChecked << 2 | mediumLevelChecked << 1 | highLevelChecked;
        if (levelCurrentValue != btnGroupLevel) {
            int xorValue = levelCurrentValue ^ btnGroupLevel;
            levelCurrentValue = xorValue;
            lowLevelChecked = xorValue & (1 << 2);
            mediumLevelChecked = xorValue & (1 << 1);
            highLevelChecked = xorValue & 1;
            checkboxChanged = true;
        }

        // play button group
        int btnGroupPlay = livePlayChecked << 1 | playbackChecked;
        if (playCurrentValue != btnGroupPlay) {
            int xorValue = playCurrentValue ^ btnGroupPlay;
            playCurrentValue = xorValue;
            livePlayChecked = xorValue & (1 << 1);
            playbackChecked = xorValue & 1;
        }

        // TODO: set camera mode here
        if (checkboxChanged) {
            int selectedMode =
                (2 - static_cast<int>(std::log2(modeCurrentValue)));
            m_ctrl->setMode(modes[selectedMode]);
        }

        cvui::beginColumn(frame, 50, 20);
        cvui::space(10);
        cvui::text("Mode: ", 0.6);
        cvui::space(10);
        cvui::beginRow(frame, 50, 60);
        cvui::checkbox("Near", &nearModeChecked);
        cvui::space(10);
        cvui::checkbox("Medium", &mediumModeChecked);
        cvui::space(10);
        cvui::checkbox("Far", &farModeChecked);
        cvui::endRow();
        cvui::endColumn();

        cvui::text(frame, 50, 100, "Video: ", 0.6);

        if (cvui::button(frame, 50, 125, 90, 30,
                         (captureEnabled ? "Stop" : "Play"))) {
            if (livePlayChecked) {
                if (m_ctrl->hasCamera()) {
                    captureEnabled = !captureEnabled;
                    if (captureEnabled) {
                        m_ctrl->startCapture();
                        m_ctrl->requestFrame();
                        status = "Playing from device!";
                    } else {
                        cv::destroyWindow(windows[1]);
                        cv::destroyWindow(windows[2]);
                        m_ctrl->stopCapture();
                        status = "";
                    }
                } else {
                    status = "No cameras connected!";
                }
            } else {
                if (fileName.empty()) {
                    fieldColor = errorColor;
                    status = "Enter a valid file name!";
                } else {
                    playbackEnabled = !playbackEnabled;
                    static std::string oldStatus = "";
                    captureEnabled = !captureEnabled;
                    if (playbackEnabled) {
                        oldStatus = status;
                        status = "Playing from file: " + fileName;
                        numberOfFrames =
                            m_ctrl->startPlayback(fileName, displayFps);
                    } else {
                        status = oldStatus;
                        m_ctrl->stopPlayback();
                        cv::destroyWindow(windows[1]);
                        cv::destroyWindow(windows[2]);
                    }
                }
            }
        }
        if (cvui::button(frame, 150, 125, 90, 30, "Record")) {
            if (fileName.empty()) {
                status = "Enter a file name!";
            } else if (!captureEnabled) {
                status = "Start live playing before recording!";
            } else {
                recordEnabled = !recordEnabled;
                static std::string oldStatus = "";
                if (recordEnabled) {
                    oldStatus = status;
                    status = "Recording into " + fileName;
                    m_ctrl->startRecording(
                        fileName, frameHeight, frameWidth,
                        static_cast<unsigned int>(displayFps));
                } else {
                    m_ctrl->stopRecording();
                    status = oldStatus;
                }
            }
        }

        cvui::rect(frame, 50, 165, 190, 30, fieldColor);
        cvui::text(frame, 60, 172, fileName);

        cvui::text(frame, 50, 202, status);
        cvui::beginRow(frame, 50, 220);
        cvui::checkbox("Live", &livePlayChecked);
        cvui::space(10);
        cvui::checkbox("Playback", &playbackChecked);
        cvui::endRow();

        static int currentFrame = 0;
        if (playbackEnabled) {
            int x = currentFrame++;
            cvui::trackbar(frame, 250, 160, 120, &x, 0, numberOfFrames + 1, 1);
        } else {
            currentFrame = 0;
        }

        int fileNameField = cvui::iarea(50, 165, 190, 30);

        if (fileNameField == cvui::CLICK) {
            fieldColor = selectedColor;
            fileNameFieldSelected = true;
        } else if (cvui::mouse(cvui::CLICK) && fileNameField != cvui::CLICK) {
            fieldColor = normalColor;
            fileNameFieldSelected = false;
        }

#ifdef SHOW_ADVANCED_GUI
        cvui::beginColumn(frame, 300, 20);
        cvui::space(10);
        cvui::text("Level: ", 0.6);
        cvui::space(10);
        cvui::checkbox("Low", &lowLevelChecked);
        cvui::space(10);
        cvui::checkbox("Medium", &mediumLevelChecked);
        cvui::space(10);
        cvui::checkbox("High", &highLevelChecked);
        cvui::endColumn();

        cvui::beginColumn(frame, 50, 140);
        cvui::space(10);
        cvui::text("Pulse Count: ", 0.6);
        cvui::space(10);
        cvui::trackbar(300, &pulseCount, 0, 2500, 1);
        cvui::endColumn();
        // TODO: set pulse count here
        // m_ctrl->setPulseCount(pulseCount);

        cvui::beginColumn(frame, 50, 240);
        cvui::space(20);
        cvui::text("Registers:", 0.6);
        cvui::space(20);
        cvui::text("Address:");
        cvui::space(25);
        cvui::text("Value:");
        cvui::endColumn();

        // Draw input rectangles and text displays for address / value
        cvui::text(frame, 125, 295, address);
        cvui::text(frame, 125, 330, value);

        cvui::rect(frame, 120, 287, 90, 30, addressColor);
        cvui::rect(frame, 120, 322, 90, 30, valueColor);

        int statusAddress = cvui::iarea(120, 287, 90, 30);
        int statusValue = cvui::iarea(120, 322, 90, 30);

        if (statusAddress == cvui::CLICK) {
            addressFieldSelected = true;
            valueFieldSelected = false;
            addressColor = selectedColor;
            valueColor = normalColor;
        }
        if (statusValue == cvui::CLICK) {
            valueFieldSelected = true;
            addressFieldSelected = false;
            valueColor = selectedColor;
            addressColor = normalColor;
        }

        if (cvui::mouse(cvui::CLICK) && statusAddress != cvui::CLICK &&
            statusValue != cvui::CLICK) {
            valueFieldSelected = false;
            addressFieldSelected = false;
            valueColor = normalColor;
            addressColor = normalColor;
        }

        if (cvui::button(frame, 225, 287, 90, 30, "Read")) {
            if (address.size() <= 2) {
                addressColor = errorColor;
            } else {
                uint16_t addr = detail::fromHexToUint(address);
                uint16_t data = 0;
                // TODO: read afe reg here
                // m_ctrl->readAFEregister(&addr, &data);
                // value = "0x" + std::to_string(data);
            }
            valueFieldSelected = false;
            addressFieldSelected = false;
        }
        if (cvui::button(frame, 225, 322, 90, 30, "Write")) {
            if (address.size() <= 2) {
                addressColor = errorColor;
            }
            if (value.size() <= 2) {
                valueColor = errorColor;
            }
            if (address.size() > 2 && value.size() > 2) {
                // TODO: write afe reg here!
                // uint16 addr = detail::fromHexToUint(address);
                // uint16 data = detail::fromHexToUint(value);
                // m_ctrl->writeAFEregister(&addr, &data);
            }
            valueFieldSelected = false;
            addressFieldSelected = false;
        }
#endif
        if (displayFps && captureEnabled) {
            cvui::text(frame, 350, 380, "FPS:" + std::to_string(displayFps));
        }

        if (captureEnabled) {
            if (temp_cnt++ == 64) {
                std::pair<float, float> temp = m_ctrl->getTemperature();
                temp_cnt = 0;
                sprintf(afe_temp_str, "AFE TEMP: %.1f", temp.first);
                sprintf(laser_temp_str, "LASER TEMP: %.1f", temp.second);
            }
            cvui::text(frame, 20, 380, afe_temp_str);
            cvui::text(frame, 180, 380, laser_temp_str);
        }

        if (captureEnabled && !playbackEnabled) {
            frameCount++;
            auto endTime = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed = endTime - startTime;
            if (elapsed.count() >= 2) {
                displayFps = frameCount / elapsed.count();
                frameCount = 0;
                startTime = endTime;
            }
        }

        cvui::beginColumn(frame, 50, 250);
        cvui::space(10);
        cvui::text("Settings: ", 0.6);
        cvui::space(10);
        cvui::checkbox("Center Point", &m_center);
        cvui::endColumn();

        cvui::imshow(m_viewName, frame);

        if (captureEnabled) {

            if (playbackEnabled) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1000 / displayFps));
            }

            if (m_ctrl->playbackFinished() && playbackEnabled) {
                captureEnabled = false;
                playbackEnabled = false;
                status = "Finished playing from: " + fileName;
                m_ctrl->stopPlayback();
                cv::destroyWindow(windows[1]);
                cv::destroyWindow(windows[2]);
            } else {
                m_capturedFrame = m_ctrl->getFrame();

                aditof::FrameDetails frameDetails;
                m_capturedFrame->getDetails(frameDetails);
                frameWidth = frameDetails.width;
                frameHeight = frameDetails.height;

                std::unique_lock<std::mutex> lock(m_frameCapturedMutex);
                m_depthFrameAvailable = true;
                m_irFrameAvailable = true;
                lock.unlock();
                m_frameCapturedCv.notify_all();
                m_ctrl->requestFrame();
            }
        }

        std::unique_lock<std::mutex> imshow_lock(m_imshowMutex);
        if (captureEnabled) {
            m_barrierCv.wait(imshow_lock,
                             [&]() { return m_waitKeyBarrier == 2; });
            m_waitKeyBarrier = 0;
        }

        if (captureEnabled) {
            cvui::imshow("Depth Image", m_depthImage);
            cvui::imshow("IR Image", m_irImage);
            m_depthImage.release();
            m_irImage.release();
        }

        int key = cv::waitKey(10);

        bool backspace = false;
        std::string pressedValidKey = detail::getKeyPressed(key, backspace);

        if (valueFieldSelected) {
            if (!backspace && value.size() < 6) {
                value += pressedValidKey;
            } else if (backspace && value.size() > 2) {
                value = value.substr(0, value.size() - 1);
            }
        }

        if (addressFieldSelected) {
            if (!backspace && address.size() < 6) {
                address += pressedValidKey;
            } else if (backspace && address.size() > 2) {
                address = address.substr(0, address.size() - 1);
            }
        }

        if (fileNameFieldSelected) {
            if (!backspace) {
                fileName += pressedValidKey;
            } else if (fileName.size() > 0) {
                fileName = fileName.substr(0, fileName.size() - 1);
            }
        }

        // Check if ESC key was pressed
        if (key == 27) {
            break;
        }
    }
    m_ctrl->stopCapture();
}

void AdiTofDemoView::_displayDepthImage() {
    while (!m_stopWorkersFlag) {
        std::unique_lock<std::mutex> lock(m_frameCapturedMutex);
        m_frameCapturedCv.wait(
            lock, [&]() { return m_depthFrameAvailable || m_stopWorkersFlag; });

        if (m_stopWorkersFlag) {
            break;
        }

        m_depthFrameAvailable = false;

        std::shared_ptr<aditof::Frame> localFrame = m_capturedFrame;
        lock.unlock(); // Lock is no longer needed

        uint16_t *data;
        localFrame->getData(aditof::FrameDataType::DEPTH, &data);

        aditof::FrameDetails frameDetails;
        localFrame->getDetails(frameDetails);

        int frameHeight = static_cast<int>(frameDetails.height) / 2;
        int frameWidth = static_cast<int>(frameDetails.width);

        m_depthImage = cv::Mat(frameHeight, frameWidth, CV_16UC1, data);
        cv::Point2d pointxy(320, 240);
        m_distanceVal = static_cast<int>(
            m_distanceVal * 0.7 + m_depthImage.at<ushort>(pointxy) * 0.3);
        char text[20];
        sprintf(text, "%d", m_distanceVal);
        m_depthImage.convertTo(m_depthImage, CV_8U, 255.0 / m_ctrl->getRange());
        applyColorMap(m_depthImage, m_depthImage, cv::COLORMAP_RAINBOW);
        flip(m_depthImage, m_depthImage, 1);
        int color;
        if (m_distanceVal > 2500)
            color = 0;
        else
            color = 4096;

        std::unique_lock<std::mutex> imshow_lock(m_imshowMutex);
        if (m_center) {
            cv::circle(m_depthImage, pointxy, 4,
                       cv::Scalar(color, color, color), -1, 8, 0);
            cv::putText(m_depthImage, text, pointxy, cv::FONT_HERSHEY_DUPLEX, 2,
                        cv::Scalar(color, color, color));
        }
        m_waitKeyBarrier += 1;
        if (m_waitKeyBarrier == 2) {
            imshow_lock.unlock();
            m_barrierCv.notify_one();
        }
    }
}

void AdiTofDemoView::_displayIrImage() {
    while (!m_stopWorkersFlag) {
        std::unique_lock<std::mutex> lock(m_frameCapturedMutex);
        m_frameCapturedCv.wait(
            lock, [&]() { return m_irFrameAvailable || m_stopWorkersFlag; });

        if (m_stopWorkersFlag) {
            break;
        }

        m_irFrameAvailable = false;

        std::shared_ptr<aditof::Frame> localFrame = m_capturedFrame;
        lock.unlock(); // Lock is no longer needed

        uint16_t *irData;
        localFrame->getData(aditof::FrameDataType::IR, &irData);

        aditof::FrameDetails frameDetails;
        localFrame->getDetails(frameDetails);

        int frameHeight = static_cast<int>(frameDetails.height) / 2;
        int frameWidth = static_cast<int>(frameDetails.width);

        m_irImage = cv::Mat(frameHeight, frameWidth, CV_16UC1, irData);
        m_irImage.convertTo(m_irImage, CV_8U, 255.0 / m_ctrl->getRange());
        flip(m_irImage, m_irImage, 1);

        std::unique_lock<std::mutex> imshow_lock(m_imshowMutex);
        m_waitKeyBarrier += 1;
        if (m_waitKeyBarrier == 2) {
            imshow_lock.unlock();
            m_barrierCv.notify_one();
        }
    }
}
