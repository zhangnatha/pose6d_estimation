/**
 * @file CameraBase.cpp
 * @brief 相机抽象接口与内参工具实现
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include "CameraBase.h"

#include <sstream>

#include "OrbbecCamera.h"
#include "RealSenseCamera.h"

namespace aruco_camera {

/**
 * @brief 根据来源选择通过 SDK 或 YAML 文件获取相机内参。
 * @param[in] source 内参来源枚举。
 * @param[in] yamlPath YAML 标定文件路径。
 * @param[out] intrinsics 输出的相机内参。
 * @param[out] error 获取失败时返回错误信息。
 * @return 获取成功返回 true，否则返回 false。
 */
bool CameraBase::getIntrinsics(IntrinsicsSource source,
                               const std::string& yamlPath,
                               CameraIntrinsics& intrinsics,
                               std::string& error) {
    if (source == IntrinsicsSource::Sdk) {
        return getSdkIntrinsics(intrinsics, error);
    }
    return loadIntrinsicsFromYaml(yamlPath, intrinsics, error);
}

/**
 * @brief 从 OpenCV YAML 文件中读取相机矩阵与畸变系数。
 * @param[in] yamlPath 标定文件路径。
 * @param[out] intrinsics 输出的相机内参。
 * @param[out] error 读取失败时返回错误信息。
 * @return 读取成功返回 true，否则返回 false。
 */
bool CameraBase::loadIntrinsicsFromYaml(const std::string& yamlPath,
                                        CameraIntrinsics& intrinsics,
                                        std::string& error) {
    cv::FileStorage fs(yamlPath, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        error = "Cannot open calibration file: " + yamlPath;
        return false;
    }

    fs["CameraMatrix"] >> intrinsics.cameraMatrix;
    fs["DistCoeffs"] >> intrinsics.distCoeffs;
    if (intrinsics.cameraMatrix.empty()) {
        fs["camera_matrix"] >> intrinsics.cameraMatrix;
    }
    if (intrinsics.distCoeffs.empty()) {
        fs["distortion_coefficients"] >> intrinsics.distCoeffs;
    }
    if (intrinsics.cameraMatrix.empty() || intrinsics.distCoeffs.empty()) {
        error = "Calibration file missing camera matrix or distortion coefficients: " + yamlPath;
        return false;
    }
    if (intrinsics.distCoeffs.rows != 1 && intrinsics.distCoeffs.cols == 1) {
        intrinsics.distCoeffs = intrinsics.distCoeffs.t();
    }
    return true;
}

/**
 * @brief 由焦距、主点和畸变数组构造统一的内参对象。
 * @param[in] fx x 方向焦距。
 * @param[in] fy y 方向焦距。
 * @param[in] cx 主点 x 坐标。
 * @param[in] cy 主点 y 坐标。
 * @param[in] distortion 畸变参数数组。
 * @return 构造后的相机内参对象。
 */
CameraIntrinsics CameraBase::makeIntrinsics(double fx,
                                            double fy,
                                            double cx,
                                            double cy,
                                            const std::vector<double>& distortion) {
    CameraIntrinsics intrinsics;
    intrinsics.cameraMatrix = (cv::Mat_<double>(3, 3) << fx, 0.0, cx,
                                                         0.0, fy, cy,
                                                         0.0, 0.0, 1.0);
    intrinsics.distCoeffs = cv::Mat::zeros(1, static_cast<int>(distortion.size()), CV_64F);
    for (size_t i = 0; i < distortion.size(); ++i) {
        intrinsics.distCoeffs.at<double>(0, static_cast<int>(i)) = distortion[i];
    }
    return intrinsics;
}

/**
 * @brief 按相机类型字符串创建对应的相机实现对象。
 * @param[in] cameraType 相机类型字符串。
 * @return 成功时返回相机对象，失败时返回空指针。
 */
std::unique_ptr<CameraBase> createCamera(const std::string& cameraType) {
    if (cameraType == "realsense") {
        return std::unique_ptr<CameraBase>(new RealSenseCamera());
    }
    if (cameraType == "orbbec") {
        return std::unique_ptr<CameraBase>(new OrbbecCamera());
    }
    return std::unique_ptr<CameraBase>();
}

/**
 * @brief 将字符串解析为内参来源枚举。
 * @param[in] text 输入字符串，如 `sdk` 或 `yaml`。
 * @param[out] source 解析得到的内参来源。
 * @return 解析成功返回 true，否则返回 false。
 */
bool parseIntrinsicsSource(const std::string& text, IntrinsicsSource& source) {
    if (text == "sdk") {
        source = IntrinsicsSource::Sdk;
        return true;
    }
    if (text == "yaml") {
        source = IntrinsicsSource::Yaml;
        return true;
    }
    return false;
}

}  // 相机命名空间
