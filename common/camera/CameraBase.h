/**
 * @file CameraBase.h
 * @brief 相机抽象接口与内参数据结构声明
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace aruco_camera {

enum class IntrinsicsSource {
    Sdk,
    Yaml
};

struct CameraIntrinsics {
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;

    /**
     * @brief 判断当前内参对象是否已包含有效的相机矩阵和畸变系数。
     * @return 有效时返回 true，否则返回 false。
     */
    bool valid() const {
        return !cameraMatrix.empty() && !distCoeffs.empty();
    }
};

struct CameraStreamConfig {
    int width;
    int height;
    int fps;
};

class CameraBase {
public:
    /**
     * @brief 析构相机基类对象。
     */
    virtual ~CameraBase() {}

    /**
     * @brief 按给定视频流配置启动相机。
     * @param[in] streamConfig 视频流分辨率与帧率配置。
     * @param[out] error 启动失败时返回错误信息。
     * @return 启动成功返回 true，否则返回 false。
     */
    virtual bool start(const CameraStreamConfig& streamConfig, std::string& error) = 0;
    /**
     * @brief 采集一帧彩色图像。
     * @param[out] frame 采集得到的 BGR 图像。
     * @param[out] error 采集失败时返回错误信息。
     * @return 采集成功返回 true，否则返回 false。
     */
    virtual bool grabFrame(cv::Mat& frame, std::string& error) = 0;
    /**
     * @brief 从相机 SDK 读取内参。
     * @param[out] intrinsics 输出的相机内参。
     * @param[out] error 读取失败时返回错误信息。
     * @return 读取成功返回 true，否则返回 false。
     */
    virtual bool getSdkIntrinsics(CameraIntrinsics& intrinsics, std::string& error) = 0;
    /**
     * @brief 停止相机并释放底层资源。
     */
    virtual void stop() = 0;
    /**
     * @brief 获取当前相机实现的名称。
     * @return 相机名称字符串。
     */
    virtual std::string cameraName() const = 0;

    /**
     * @brief 按指定来源获取相机内参。
     * @param[in] source 内参来源，支持 SDK 或 YAML 文件。
     * @param[in] yamlPath 当来源为 YAML 时使用的配置文件路径。
     * @param[out] intrinsics 输出的相机内参。
     * @param[out] error 获取失败时返回错误信息。
     * @return 获取成功返回 true，否则返回 false。
     */
    bool getIntrinsics(IntrinsicsSource source,
                       const std::string& yamlPath,
                       CameraIntrinsics& intrinsics,
                       std::string& error);

    /**
     * @brief 从 YAML 标定文件加载相机内参。
     * @param[in] yamlPath 标定文件路径。
     * @param[out] intrinsics 输出的相机内参。
     * @param[out] error 加载失败时返回错误信息。
     * @return 加载成功返回 true，否则返回 false。
     */
    static bool loadIntrinsicsFromYaml(const std::string& yamlPath,
                                       CameraIntrinsics& intrinsics,
                                       std::string& error);

protected:
    /**
     * @brief 根据焦距、主点和畸变参数构造内参对象。
     * @param[in] fx x 方向焦距。
     * @param[in] fy y 方向焦距。
     * @param[in] cx 主点 x 坐标。
     * @param[in] cy 主点 y 坐标。
     * @param[in] distortion 畸变参数数组。
     * @return 组装后的相机内参对象。
     */
    static CameraIntrinsics makeIntrinsics(double fx,
                                           double fy,
                                           double cx,
                                           double cy,
                                           const std::vector<double>& distortion);
};

/**
 * @brief 按类型创建具体相机实例。
 * @param[in] cameraType 相机类型字符串，如 `realsense` 或 `orbbec`。
 * @return 成功时返回相机对象，失败时返回空指针。
 */
std::unique_ptr<CameraBase> createCamera(const std::string& cameraType);
/**
 * @brief 解析内参来源字符串。
 * @param[in] text 输入字符串，如 `sdk` 或 `yaml`。
 * @param[out] source 解析后的内参来源枚举。
 * @return 解析成功返回 true，否则返回 false。
 */
bool parseIntrinsicsSource(const std::string& text, IntrinsicsSource& source);

}  // 相机命名空间
