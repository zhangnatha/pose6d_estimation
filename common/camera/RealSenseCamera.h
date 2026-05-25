/**
 * @file RealSenseCamera.h
 * @brief RealSense 相机封装接口声明
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#pragma once

#include "CameraBase.h"

#include <librealsense2/rs.hpp>

namespace aruco_camera {

class RealSenseCamera : public CameraBase {
public:
    /**
     * @brief 构造 RealSense 相机对象。
     */
    RealSenseCamera();
    /**
     * @brief 析构 RealSense 相机对象，并停止当前流。
     */
    ~RealSenseCamera() override;

    /**
     * @brief 启动 RealSense 彩色流。
     * @param[in] streamConfig 视频流配置。
     * @param[out] error 启动失败时返回错误信息。
     * @return 启动成功返回 true，否则返回 false。
     */
    bool start(const CameraStreamConfig& streamConfig, std::string& error) override;
    /**
     * @brief 采集一帧 RealSense 彩色图像。
     * @param[out] frame 输出的 BGR 图像。
     * @param[out] error 采集失败时返回错误信息。
     * @return 采集成功返回 true，否则返回 false。
     */
    bool grabFrame(cv::Mat& frame, std::string& error) override;
    /**
     * @brief 从 RealSense SDK 读取当前彩色相机内参。
     * @param[out] intrinsics 输出的相机内参。
     * @param[out] error 读取失败时返回错误信息。
     * @return 读取成功返回 true，否则返回 false。
     */
    bool getSdkIntrinsics(CameraIntrinsics& intrinsics, std::string& error) override;
    /**
     * @brief 停止 RealSense 相机流。
     */
    void stop() override;
    /**
     * @brief 获取当前相机名称。
     * @return 固定返回 `RealSense`。
     */
    std::string cameraName() const override;

private:
    rs2::pipeline pipeline_;
    rs2::config config_;
    rs2::pipeline_profile profile_;
    bool started_;
};

}  // 相机命名空间
