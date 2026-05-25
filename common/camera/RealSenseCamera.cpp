/**
 * @file RealSenseCamera.cpp
 * @brief RealSense 相机采集与内参读取实现
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include "RealSenseCamera.h"

#include <exception>

namespace aruco_camera {

/**
 * @brief 构造 RealSense 相机对象并初始化运行状态。
 */
RealSenseCamera::RealSenseCamera() : started_(false) {}

/**
 * @brief 析构 RealSense 相机对象，并停止当前数据流。
 */
RealSenseCamera::~RealSenseCamera() {
    stop();
}

/**
 * @brief 按给定分辨率和帧率启动 RealSense 彩色流。
 * @param[in] streamConfig 期望的视频流配置。
 * @param[out] error 启动失败时返回错误信息。
 * @return 启动成功返回 true，否则返回 false。
 */
bool RealSenseCamera::start(const CameraStreamConfig& streamConfig, std::string& error) {
    try {
        if (streamConfig.width > 0 && streamConfig.height > 0 && streamConfig.fps > 0) {
            config_.enable_stream(RS2_STREAM_COLOR,
                                  streamConfig.width,
                                  streamConfig.height,
                                  RS2_FORMAT_BGR8,
                                  streamConfig.fps);
        } else {
            config_.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);
        }
        profile_ = pipeline_.start(config_);
        started_ = true;
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        started_ = false;
        return false;
    }
}

/**
 * @brief 采集一帧 RealSense 彩色图像。
 * @param[out] frame 输出的 BGR 图像。
 * @param[out] error 采集失败时返回错误信息。
 * @return 采集成功返回 true，否则返回 false。
 */
bool RealSenseCamera::grabFrame(cv::Mat& frame, std::string& error) {
    if (!started_) {
        error = "RealSense camera is not started.";
        return false;
    }

    try {
        rs2::frameset frames = pipeline_.wait_for_frames();
        rs2::video_frame colorFrame = frames.get_color_frame();
        if (!colorFrame) {
            error = "Failed to get RealSense color frame.";
            return false;
        }

        cv::Mat raw(cv::Size(colorFrame.get_width(), colorFrame.get_height()),
                    CV_8UC3,
                    const_cast<void*>(colorFrame.get_data()),
                    cv::Mat::AUTO_STEP);
        frame = raw.clone();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

/**
 * @brief 从 RealSense SDK 当前彩色流中读取相机内参。
 * @param[out] intrinsics 输出的相机内参。
 * @param[out] error 读取失败时返回错误信息。
 * @return 读取成功返回 true，否则返回 false。
 */
bool RealSenseCamera::getSdkIntrinsics(CameraIntrinsics& intrinsics, std::string& error) {
    if (!started_) {
        error = "RealSense camera is not started.";
        return false;
    }

    try {
        rs2_intrinsics rsIntrinsics = profile_.get_stream(RS2_STREAM_COLOR)
                                          .as<rs2::video_stream_profile>()
                                          .get_intrinsics();
        intrinsics = makeIntrinsics(rsIntrinsics.fx,
                                    rsIntrinsics.fy,
                                    rsIntrinsics.ppx,
                                    rsIntrinsics.ppy,
                                    std::vector<double>{
                                        rsIntrinsics.coeffs[0],
                                        rsIntrinsics.coeffs[1],
                                        rsIntrinsics.coeffs[2],
                                        rsIntrinsics.coeffs[3],
                                        rsIntrinsics.coeffs[4]
                                    });
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

/**
 * @brief 停止 RealSense 相机流。
 */
void RealSenseCamera::stop() {
    if (started_) {
        try {
            pipeline_.stop();
        } catch (...) {
        }
        started_ = false;
    }
}

/**
 * @brief 返回当前相机实现名称。
 * @return 固定返回 `RealSense`。
 */
std::string RealSenseCamera::cameraName() const {
    return "RealSense";
}

}  // 相机命名空间
