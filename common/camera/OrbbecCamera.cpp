/**
 * @file OrbbecCamera.cpp
 * @brief Orbbec 相机采集与内参读取实现
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include "OrbbecCamera.h"

#include <exception>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace aruco_camera {

/**
 * @brief 构造 Orbbec 相机对象并初始化状态。
 */
OrbbecCamera::OrbbecCamera() : started_(false) {}

/**
 * @brief 析构 Orbbec 相机对象，并主动停止底层流水线。
 */
OrbbecCamera::~OrbbecCamera() {
    stop();
}

/**
 * @brief 启动 Orbbec 彩色流并缓存当前彩色流配置。
 * @param[in] streamConfig 期望的视频流配置。
 * @param[out] error 启动失败时返回错误信息。
 * @return 启动成功返回 true，否则返回 false。
 */
bool OrbbecCamera::start(const CameraStreamConfig& streamConfig, std::string& error) {
    try {
        pipeline_.reset(new ob::Pipeline());
        config_.reset(new ob::Config());

        try {
            const uint32_t width = (streamConfig.width > 0) ? static_cast<uint32_t>(streamConfig.width) : OB_WIDTH_ANY;
            const uint32_t height = (streamConfig.height > 0) ? static_cast<uint32_t>(streamConfig.height) : OB_HEIGHT_ANY;
            const uint32_t fps = (streamConfig.fps > 0) ? static_cast<uint32_t>(streamConfig.fps) : OB_FPS_ANY;
            config_->enableVideoStream(OB_STREAM_COLOR, width, height, fps, OB_FORMAT_BGR);
        } catch (...) {
            config_->enableVideoStream(OB_STREAM_COLOR);
        }

        pipeline_->start(config_);
        std::shared_ptr<ob::FrameSet> frameSet = pipeline_->waitForFrameset(1000);
        if (frameSet == nullptr) {
            error = "Timed out waiting for Orbbec frameset.";
            stop();
            return false;
        }

        std::shared_ptr<ob::Frame> colorBaseFrame = frameSet->getFrame(OB_FRAME_COLOR);
        if (colorBaseFrame == nullptr) {
            error = "Orbbec frameset does not contain a color frame.";
            stop();
            return false;
        }

        std::shared_ptr<ob::ColorFrame> colorFrame = colorBaseFrame->as<ob::ColorFrame>();
        if (colorFrame == nullptr) {
            error = "Failed to get Orbbec color frame.";
            stop();
            return false;
        }

        std::shared_ptr<ob::StreamProfile> colorStreamProfile = colorFrame->getStreamProfile();
        if (colorStreamProfile == nullptr) {
            error = "Failed to get Orbbec color stream profile.";
            stop();
            return false;
        }

        colorProfile_ = colorStreamProfile->as<ob::VideoStreamProfile>();
        if (colorProfile_ == nullptr) {
            error = "Failed to get Orbbec color stream profile.";
            stop();
            return false;
        }

        started_ = true;
        return true;
    } catch (const ob::Error& ex) {
        error = ex.what();
        stop();
        return false;
    } catch (const std::exception& ex) {
        error = ex.what();
        stop();
        return false;
    }
}

/**
 * @brief 采集一帧 Orbbec 彩色图像并转换为 BGR 格式。
 * @param[out] frame 输出的 BGR 图像。
 * @param[out] error 采集或转换失败时返回错误信息。
 * @return 成功返回 true，否则返回 false。
 */
bool OrbbecCamera::grabFrame(cv::Mat& frame, std::string& error) {
    if (!started_ || !pipeline_) {
        error = "Orbbec camera is not started.";
        return false;
    }

    try {
        std::shared_ptr<ob::FrameSet> frameSet = pipeline_->waitForFrameset(1000);
        if (frameSet == nullptr) {
            error = "Timed out waiting for Orbbec frameset.";
            return false;
        }

        std::shared_ptr<ob::Frame> colorBaseFrame = frameSet->getFrame(OB_FRAME_COLOR);
        if (colorBaseFrame == nullptr) {
            error = "Orbbec frameset does not contain a color frame.";
            return false;
        }

        std::shared_ptr<ob::ColorFrame> colorFrame = colorBaseFrame->as<ob::ColorFrame>();
        if (colorFrame == nullptr) {
            error = "Failed to get Orbbec color frame.";
            return false;
        }

        if (!colorProfile_) {
            std::shared_ptr<ob::StreamProfile> colorStreamProfile = colorFrame->getStreamProfile();
            if (colorStreamProfile == nullptr) {
                error = "Failed to get Orbbec color stream profile.";
                return false;
            }
            colorProfile_ = colorStreamProfile->as<ob::VideoStreamProfile>();
            if (!colorProfile_) {
                error = "Failed to cast Orbbec color stream profile to video stream profile.";
                return false;
            }
        }

        frame = colorFrameToBgr(colorFrame, error);
        return !frame.empty();
    } catch (const ob::Error& ex) {
        error = ex.what();
        return false;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

/**
 * @brief 从 Orbbec SDK 当前彩色流配置中读取相机内参。
 * @param[out] intrinsics 输出的相机内参。
 * @param[out] error 读取失败时返回错误信息。
 * @return 读取成功返回 true，否则返回 false。
 */
bool OrbbecCamera::getSdkIntrinsics(CameraIntrinsics& intrinsics, std::string& error) {
    if (!started_ || !colorProfile_) {
        error = "Orbbec color profile is not available.";
        return false;
    }

    try {
        OBCameraIntrinsic camera = colorProfile_->getIntrinsic();
        OBCameraDistortion distortion = colorProfile_->getDistortion();
        intrinsics = makeIntrinsics(camera.fx,
                                    camera.fy,
                                    camera.cx,
                                    camera.cy,
                                    std::vector<double>{
                                        distortion.k1,
                                        distortion.k2,
                                        distortion.p1,
                                        distortion.p2,
                                        distortion.k3,
                                        distortion.k4,
                                        distortion.k5,
                                        distortion.k6
                                    });
        return true;
    } catch (const ob::Error& ex) {
        error = ex.what();
        return false;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

/**
 * @brief 停止 Orbbec 相机并释放已持有的 SDK 资源。
 */
void OrbbecCamera::stop() {
    if (pipeline_) {
        try {
            pipeline_->stop();
        } catch (...) {
        }
    }
    colorProfile_.reset();
    config_.reset();
    pipeline_.reset();
    started_ = false;
}

/**
 * @brief 返回当前相机实现名称。
 * @return 固定返回 `Orbbec`。
 */
std::string OrbbecCamera::cameraName() const {
    return "Orbbec";
}

/**
 * @brief 将 Orbbec SDK 彩色帧转换为 OpenCV BGR 图像。
 * @param[in] frame Orbbec 彩色帧对象。
 * @param[out] error 转换失败时返回错误信息。
 * @return 转换后的 BGR 图像；失败时返回空图像。
 */
cv::Mat OrbbecCamera::colorFrameToBgr(const std::shared_ptr<ob::ColorFrame>& frame, std::string& error) {
    if (!frame) {
        error = "Null Orbbec color frame.";
        return cv::Mat();
    }

    std::shared_ptr<ob::VideoFrame> videoFrame = frame->as<ob::VideoFrame>();
    if (!videoFrame) {
        error = "Failed to cast Orbbec color frame to video frame.";
        return cv::Mat();
    }
    cv::Mat converted;

    switch (videoFrame->getFormat()) {
        case OB_FORMAT_MJPG: {
            cv::Mat raw(1, videoFrame->getDataSize(), CV_8UC1, videoFrame->getData());
            converted = cv::imdecode(raw, cv::IMREAD_COLOR);
            break;
        }
        case OB_FORMAT_BGR: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC3, videoFrame->getData());
            converted = raw.clone();
            break;
        }
        case OB_FORMAT_RGB: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC3, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_RGB2BGR);
            break;
        }
        case OB_FORMAT_BGRA: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC4, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_BGRA2BGR);
            break;
        }
        case OB_FORMAT_RGBA: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC4, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_RGBA2BGR);
            break;
        }
        case OB_FORMAT_NV21: {
            cv::Mat raw(videoFrame->getHeight() * 3 / 2, videoFrame->getWidth(), CV_8UC1, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_YUV2BGR_NV21);
            break;
        }
        case OB_FORMAT_I420: {
            cv::Mat raw(videoFrame->getHeight() * 3 / 2, videoFrame->getWidth(), CV_8UC1, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_YUV2BGR_I420);
            break;
        }
        case OB_FORMAT_YUYV:
        case OB_FORMAT_YUY2: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC2, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_YUV2BGR_YUY2);
            break;
        }
        case OB_FORMAT_UYVY: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC2, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_YUV2BGR_UYVY);
            break;
        }
        case OB_FORMAT_Y8: {
            cv::Mat raw(videoFrame->getHeight(), videoFrame->getWidth(), CV_8UC1, videoFrame->getData());
            cv::cvtColor(raw, converted, cv::COLOR_GRAY2BGR);
            break;
        }
        default:
            error = "Unsupported Orbbec color format: " + std::to_string(static_cast<int>(videoFrame->getFormat()));
            return cv::Mat();
    }

    if (converted.empty()) {
        error = "Failed to convert Orbbec color frame to BGR.";
    }
    return converted;
}

}  // 相机命名空间
