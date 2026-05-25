/**
 * @file OrbbecCamera.h
 * @brief Orbbec 相机封装接口声明
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#pragma once

#include "CameraBase.h"

#include <libobsensor/ObSensor.hpp>

namespace aruco_camera {

class OrbbecCamera : public CameraBase {
public:
    /**
     * @brief 构造 Orbbec 相机对象。
     */
    OrbbecCamera();
    /**
     * @brief 析构 Orbbec 相机对象，并确保资源被释放。
     */
    ~OrbbecCamera() override;

    /**
     * @brief 启动 Orbbec 彩色流。
     * @param[in] streamConfig 视频流配置。
     * @param[out] error 启动失败时返回错误信息。
     * @return 启动成功返回 true，否则返回 false。
     */
    bool start(const CameraStreamConfig& streamConfig, std::string& error) override;
    /**
     * @brief 采集一帧 Orbbec 彩色图像。
     * @param[out] frame 输出的 BGR 图像。
     * @param[out] error 采集失败时返回错误信息。
     * @return 采集成功返回 true，否则返回 false。
     */
    bool grabFrame(cv::Mat& frame, std::string& error) override;
    /**
     * @brief 从 Orbbec SDK 读取当前彩色相机内参。
     * @param[out] intrinsics 输出的相机内参。
     * @param[out] error 读取失败时返回错误信息。
     * @return 读取成功返回 true，否则返回 false。
     */
    bool getSdkIntrinsics(CameraIntrinsics& intrinsics, std::string& error) override;
    /**
     * @brief 停止 Orbbec 相机并释放流水线资源。
     */
    void stop() override;
    /**
     * @brief 获取当前相机名称。
     * @return 固定返回 `Orbbec`。
     */
    std::string cameraName() const override;

private:
    /**
     * @brief 将 Orbbec 彩色帧转换为 OpenCV BGR 图像。
     * @param[in] frame Orbbec 彩色帧对象。
     * @param[out] error 转换失败时返回错误信息。
     * @return 转换后的 BGR 图像。
     */
    static cv::Mat colorFrameToBgr(const std::shared_ptr<ob::ColorFrame>& frame, std::string& error);

    std::shared_ptr<ob::Pipeline> pipeline_;
    std::shared_ptr<ob::Config> config_;
    std::shared_ptr<ob::VideoStreamProfile> colorProfile_;
    bool started_;
};

}  // 相机命名空间
