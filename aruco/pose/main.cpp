/**
 * @file main.cpp
 * @brief 支持单图与实时相机模式的 ArUco 位姿估计程序
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "CameraBase.h"

namespace {

const float MARKER_SIZE_M = 0.04f;
const std::string DEFAULT_DICT_NAME = "DICT_4X4_50";
const std::string DEFAULT_CONFIG_PATH = "./config.yaml";

struct PoseConfig {
    std::string mode;
    std::string imagePath;
    std::string cameraType;
    std::string intrinsicsSource;
    std::string calibYamlPath;
    float markerSizeM;
    std::string dictionary;
    int cameraWidth;
    int cameraHeight;
    int cameraFps;
};

/**
 * @brief 判断当前运行环境是否支持 OpenCV HighGUI 窗口显示。
 * @return 支持图形显示返回 true，否则返回 false。
 */
bool canUseHighGui() {
#ifdef _WIN32
    return true;
#else
    const char* display = std::getenv("DISPLAY");
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    return (display && std::strlen(display) > 0) || (waylandDisplay && std::strlen(waylandDisplay) > 0);
#endif
}

/**
 * @brief 判断路径是否为绝对路径。
 * @param[in] path 待检查路径。
 * @return 绝对路径返回 true，否则返回 false。
 */
bool isAbsolutePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (path[0] == '/') {
        return true;
    }
    return path.size() > 1 && path[1] == ':';
}

/**
 * @brief 获取路径对应的目录部分。
 * @param[in] path 输入路径。
 * @return 路径的父目录；找不到时返回 `.`。
 */
std::string dirnameOf(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

/**
 * @brief 拼接基础目录与相对路径。
 * @param[in] base 基础目录。
 * @param[in] rel 相对路径。
 * @return 拼接后的路径字符串。
 */
std::string joinPath(const std::string& base, const std::string& rel) {
    if (base.empty() || base == ".") {
        return rel;
    }
    if (base[base.size() - 1] == '/' || base[base.size() - 1] == '\\') {
        return base + rel;
    }
    return base + "/" + rel;
}

/**
 * @brief 将字典名称字符串解析为 OpenCV ArUco 字典枚举。
 * @param[in] name 字典名称。
 * @param[out] dictId 解析得到的字典枚举。
 * @return 解析成功返回 true，否则返回 false。
 */
bool resolveDictionary(const std::string& name, cv::aruco::PREDEFINED_DICTIONARY_NAME& dictId) {
    if (name == "DICT_4X4_50") dictId = cv::aruco::DICT_4X4_50;
    else if (name == "DICT_4X4_100") dictId = cv::aruco::DICT_4X4_100;
    else if (name == "DICT_4X4_250") dictId = cv::aruco::DICT_4X4_250;
    else if (name == "DICT_4X4_1000") dictId = cv::aruco::DICT_4X4_1000;
    else if (name == "DICT_5X5_50") dictId = cv::aruco::DICT_5X5_50;
    else if (name == "DICT_5X5_100") dictId = cv::aruco::DICT_5X5_100;
    else if (name == "DICT_5X5_250") dictId = cv::aruco::DICT_5X5_250;
    else if (name == "DICT_5X5_1000") dictId = cv::aruco::DICT_5X5_1000;
    else if (name == "DICT_6X6_50") dictId = cv::aruco::DICT_6X6_50;
    else if (name == "DICT_6X6_100") dictId = cv::aruco::DICT_6X6_100;
    else if (name == "DICT_6X6_250") dictId = cv::aruco::DICT_6X6_250;
    else if (name == "DICT_6X6_1000") dictId = cv::aruco::DICT_6X6_1000;
    else if (name == "DICT_7X7_50") dictId = cv::aruco::DICT_7X7_50;
    else if (name == "DICT_7X7_100") dictId = cv::aruco::DICT_7X7_100;
    else if (name == "DICT_7X7_250") dictId = cv::aruco::DICT_7X7_250;
    else if (name == "DICT_7X7_1000") dictId = cv::aruco::DICT_7X7_1000;
    else if (name == "DICT_ARUCO_ORIGINAL") dictId = cv::aruco::DICT_ARUCO_ORIGINAL;
    else return false;
    return true;
}

/**
 * @brief 将旋转向量转换为欧拉角表示，单位为度。
 * @param[in] rvec OpenCV 旋转向量。
 * @return 欧拉角向量，按度输出。
 */
cv::Vec3d getEulerAngles(const cv::Vec3d& rvec) {
    cv::Mat rotationMatrix;
    cv::Rodrigues(rvec, rotationMatrix);

    const double sy = std::sqrt(rotationMatrix.at<double>(0, 0) * rotationMatrix.at<double>(0, 0) +
                                rotationMatrix.at<double>(1, 0) * rotationMatrix.at<double>(1, 0));
    const bool singular = sy < 1e-6;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    if (!singular) {
        x = std::atan2(rotationMatrix.at<double>(2, 1), rotationMatrix.at<double>(2, 2));
        y = std::atan2(-rotationMatrix.at<double>(2, 0), sy);
        z = std::atan2(rotationMatrix.at<double>(1, 0), rotationMatrix.at<double>(0, 0));
    } else {
        x = std::atan2(-rotationMatrix.at<double>(1, 2), rotationMatrix.at<double>(1, 1));
        y = std::atan2(-rotationMatrix.at<double>(2, 0), sy);
    }

    return cv::Vec3d(x * 180.0 / CV_PI, y * 180.0 / CV_PI, z * 180.0 / CV_PI);
}

/**
 * @brief 在图像左上角绘制检测到的位姿文本信息。
 * @param[in,out] image 输出叠加信息的图像。
 * @param[in] ids 检测到的 marker 编号列表。
 * @param[in] tvecs 平移向量列表。
 * @param[in] eulersDeg 欧拉角列表，单位为度。
 */
void drawPoseInfoTopLeft(cv::Mat& image,
                         const std::vector<int>& ids,
                         const std::vector<cv::Vec3d>& tvecs,
                         const std::vector<cv::Vec3d>& eulersDeg) {
    if (ids.empty()) {
        return;
    }

    const float scale = std::sqrt(static_cast<float>(image.cols * image.cols + image.rows * image.rows)) /
                        std::sqrt(640.0f * 640.0f + 480.0f * 480.0f);
    const double fontScale = 0.45 * scale;
    const int thickness = std::max(1, static_cast<int>(1.5f * scale));
    const int outline = std::max(2, static_cast<int>(3.0f * scale));

    int baseline = 0;
    const cv::Size textSize = cv::getTextSize("ID: 0 = 0.0,0.0,0.0 mm 0.0,0.0,0.0 deg",
                                              cv::FONT_HERSHEY_COMPLEX, fontScale, thickness, &baseline);
    const int lineHeight = textSize.height + baseline + 4;

    for (size_t i = 0; i < ids.size(); ++i) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1)
            << "ID: " << ids[i] << " = "
            << tvecs[i][0] * 1000.0 << "," << tvecs[i][1] * 1000.0 << "," << tvecs[i][2] * 1000.0
            << " mm "
            << eulersDeg[i][0] << "," << eulersDeg[i][1] << "," << eulersDeg[i][2]
            << " deg";

        const cv::Point origin(15, 30 + static_cast<int>(i) * lineHeight);
        cv::putText(image, oss.str(), origin, cv::FONT_HERSHEY_COMPLEX, fontScale, cv::Scalar(0, 0, 0), outline);
        cv::putText(image, oss.str(), origin, cv::FONT_HERSHEY_COMPLEX, fontScale, cv::Scalar(40, 180, 40), thickness);
    }
}

    /**
 * @brief 在图像上绘制 ArUco 编号标签（效果与 STag 一致）。
 * @param[in,out] image 输出图像。
 * @param[in] corners 检测到的角点集合。
 * @param[in] ids 检测到的 marker 编号。
 */
void drawDetectedMarkers(cv::Mat& image,
                         const std::vector<std::vector<cv::Point2f>>& corners,
                         const std::vector<int>& ids) {
    const bool drawIds = !ids.empty() && ids.size() == corners.size();
    for (size_t i = 0; i < corners.size(); ++i) {
        if (!drawIds) {
            continue;
        }
        const cv::Point2f center = (corners[i][0] + corners[i][2]) * 0.5f;
        const std::string label = std::to_string(ids[i]);
        cv::putText(image, label, center, cv::FONT_HERSHEY_COMPLEX, 1.0, cv::Scalar(245, 245, 245), 5);
        cv::putText(image, label, center, cv::FONT_HERSHEY_COMPLEX, 1.0, cv::Scalar(40, 180, 40), 2);
    }
}

/**
 * @brief 检测 ArUco 标记并绘制边框、自定义三轴与位姿文本。
 * （抛弃原 cv::drawFrameAxes，改为与 STag 相同的 cv::solvePnP 和 cv::projectPoints 绘制）
 * @param[in,out] image 输入输出图像。
 * @param[in] cameraMatrix 相机内参矩阵。
 * @param[in] distCoeffs 畸变系数。
 * @param[in] dictionary ArUco 字典对象。
 * @param[in] markerSizeM 标记物理边长，单位米。
 */
void processAndDrawMarkers(cv::Mat& image,
                           const cv::Mat& cameraMatrix,
                           const cv::Mat& distCoeffs,
                           const cv::Ptr<cv::aruco::Dictionary>& dictionary,
                           float markerSizeM) {
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    std::vector<std::vector<cv::Point2f>> rejected;
    cv::Ptr<cv::aruco::DetectorParameters> detectorParams = cv::aruco::DetectorParameters::create();

    cv::aruco::detectMarkers(image, dictionary, corners, ids, detectorParams, rejected);
    if (ids.empty()) {
        return;
    }

    drawDetectedMarkers(image, corners, ids);

    const float scale = std::sqrt(static_cast<float>(image.cols * image.cols + image.rows * image.rows)) /
                        std::sqrt(640.0f * 640.0f + 480.0f * 480.0f);
    const int axisThickness = std::max(2, static_cast<int>(1.5f * scale));

    const std::vector<cv::Point3f> objectPoints = {
        cv::Point3f(0.0f, 0.0f, 0.0f),
        cv::Point3f(markerSizeM, 0.0f, 0.0f),
        cv::Point3f(markerSizeM, markerSizeM, 0.0f),
        cv::Point3f(0.0f, markerSizeM, 0.0f)
    };

    const float axisLen = markerSizeM;
    const std::vector<cv::Point3f> axis3DPoints = {
        cv::Point3f(0.0f, 0.0f, 0.0f),
        cv::Point3f(axisLen, 0.0f, 0.0f),
        cv::Point3f(0.0f, axisLen, 0.0f),
        cv::Point3f(0.0f, 0.0f, -axisLen)
    };

    std::vector<cv::Vec3d> translationsMm;
    std::vector<cv::Vec3d> eulersDeg;
    translationsMm.reserve(corners.size());
    eulersDeg.reserve(corners.size());

    for (size_t i = 0; i < corners.size(); ++i) {
        cv::Mat rvec;
        cv::Mat tvec;
        cv::solvePnP(objectPoints, corners[i], cameraMatrix, distCoeffs, rvec, tvec);

        std::vector<cv::Point2f> imagePoints;
        cv::projectPoints(axis3DPoints, rvec, tvec, cameraMatrix, distCoeffs, imagePoints);

        cv::line(image, imagePoints[0], imagePoints[1], cv::Scalar(0, 0, 255), axisThickness);
        cv::line(image, imagePoints[0], imagePoints[2], cv::Scalar(0, 255, 0), axisThickness);
        cv::line(image, imagePoints[0], imagePoints[3], cv::Scalar(255, 0, 0), axisThickness);

        translationsMm.push_back(cv::Vec3d(tvec.at<double>(0) * 1000.0,
                                           tvec.at<double>(1) * 1000.0,
                                           tvec.at<double>(2) * 1000.0));
        eulersDeg.push_back(getEulerAngles(rvec));
    }

    drawPoseInfoTopLeft(image, ids, translationsMm, eulersDeg);
}

/**
 * @brief 根据输入图像路径生成输出图像路径。
 * @param[in] imagePath 原始图像路径。
 * @return 附加 `_out` 后缀的输出路径。
 */
std::string makeOutputPath(const std::string& imagePath) {
    const size_t dotPos = imagePath.find_last_of('.');
    const size_t slashPos = imagePath.find_last_of("/\\");
    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
        return imagePath.substr(0, dotPos) + "_out" + imagePath.substr(dotPos);
    }
    return imagePath + "_out.png";
}

/**
 * @brief 以单张本地图像模式执行 ArUco 位姿估计。
 * @param[in] imagePath 输入图像路径。
 * @param[in] calibPath 标定文件路径。
 * @param[in] dictionary ArUco 字典对象。
 * @param[in] markerSizeM 标记边长，单位米。
 * @return 成功返回 0，失败返回 -1。
 */
int runLocalImageMode(const std::string& imagePath,
                      const std::string& calibPath,
                      const cv::Ptr<cv::aruco::Dictionary>& dictionary,
                      float markerSizeM) {
    std::cout << "[Info] Starting local image mode..." << std::endl;
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "[Error] Cannot load image: " << imagePath << std::endl;
        return -1;
    }

    aruco_camera::CameraIntrinsics intrinsics;
    std::string error;
    if (!aruco_camera::CameraBase::loadIntrinsicsFromYaml(calibPath, intrinsics, error)) {
        std::cerr << "[Error] " << error << std::endl;
        return -1;
    }

    cv::Mat drawImage = image.clone();
    processAndDrawMarkers(drawImage, intrinsics.cameraMatrix, intrinsics.distCoeffs, dictionary, markerSizeM);

    const std::string outputPath = makeOutputPath(imagePath);
    if (!cv::imwrite(outputPath, drawImage)) {
        std::cerr << "[Error] Failed to save output image: " << outputPath << std::endl;
        return -1;
    }

    std::cout << "[Info] Processing completed, result saved as: " << outputPath << std::endl;
    std::cout << "[Info] Local image mode finished (no GUI preview)." << std::endl;
    return 0;
}

/**
 * @brief 以实时相机模式执行 ArUco 位姿估计。
 * @param[in] cameraType 相机类型字符串。
 * @param[in] intrinsicsSource 内参来源。
 * @param[in] yamlPath YAML 内参文件路径。
 * @param[in] streamConfig 相机视频流配置。
 * @param[in] dictionary ArUco 字典对象。
 * @param[in] markerSizeM 标记边长，单位米。
 * @return 成功返回 0，失败返回 -1。
 */
int runCameraMode(const std::string& cameraType,
                  aruco_camera::IntrinsicsSource intrinsicsSource,
                  const std::string& yamlPath,
                  const aruco_camera::CameraStreamConfig& streamConfig,
                  const cv::Ptr<cv::aruco::Dictionary>& dictionary,
                  float markerSizeM) {
    if (!canUseHighGui()) {
        std::cerr << "[Error] No GUI display detected. Camera live mode requires DISPLAY/WAYLAND." << std::endl;
        return -1;
    }

    std::unique_ptr<aruco_camera::CameraBase> camera = aruco_camera::createCamera(cameraType);
    if (!camera) {
        std::cerr << "[Error] Unsupported camera type: " << cameraType << std::endl;
        return -1;
    }

    std::string error;
    if (!camera->start(streamConfig, error)) {
        std::cerr << "[Error] Failed to start " << camera->cameraName() << " camera: " << error << std::endl;
        return -1;
    }

    aruco_camera::CameraIntrinsics intrinsics;
    if (!camera->getIntrinsics(intrinsicsSource, yamlPath, intrinsics, error)) {
        std::cerr << "[Error] Failed to get intrinsics: " << error << std::endl;
        camera->stop();
        return -1;
    }

    std::cout << "[Info] Starting " << camera->cameraName() << " live mode..." << std::endl;
    try {
        cv::namedWindow("Camera Live (Processed)", cv::WINDOW_AUTOSIZE);
    } catch (const cv::Exception&) {
        std::cerr << "[Error] Failed to initialize GUI backend for camera live mode." << std::endl;
        camera->stop();
        return -1;
    }

    int frameCount = 0;
    double fps = 0.0;
    std::clock_t lastTick = std::clock();

    while (true) {
        cv::Mat frame;
        if (!camera->grabFrame(frame, error)) {
            std::cerr << "[Error] Failed to grab frame: " << error << std::endl;
            break;
        }

        cv::Mat drawImage = frame.clone();
        processAndDrawMarkers(drawImage, intrinsics.cameraMatrix, intrinsics.distCoeffs, dictionary, markerSizeM);

        ++frameCount;
        std::clock_t nowTick = std::clock();
        double elapsed = static_cast<double>(nowTick - lastTick) / CLOCKS_PER_SEC;
        if (elapsed >= 0.5) {
            fps = frameCount / elapsed;
            frameCount = 0;
            lastTick = nowTick;
        }

        const std::string sourceText = (intrinsicsSource == aruco_camera::IntrinsicsSource::Sdk)
                                           ? "Intrinsics: SDK"
                                           : "Intrinsics: YAML";
        cv::rectangle(drawImage, cv::Rect(0, 0, 250, 50), cv::Scalar(0, 0, 0), -1);
        cv::putText(drawImage, "FPS: " + std::to_string(static_cast<int>(fps)), cv::Point(10, 18),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
        cv::putText(drawImage, sourceText, cv::Point(10, 40),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);

        cv::imshow("Camera Live (Processed)", drawImage);

        const int key = cv::waitKey(1);
        if (key == 27) {
            break;
        }
        if (key == 's' || key == 'S') {
            const std::time_t nowTs = std::time(NULL);
            const std::tm* localTm = std::localtime(&nowTs);
            std::ostringstream oss;
            oss << "capture_" << cameraType << "_" << std::put_time(localTm, "%Y%m%d_%H%M%S") << ".png";
            cv::imwrite(oss.str(), frame);
            std::cout << "[Info] Saved original image: " << oss.str() << std::endl;
        }
    }

    camera->stop();
    return 0;
}

/**
 * @brief 打印程序命令行使用说明。
 * @param[in] app 可执行程序名称。
 */
void printUsage(const char* app) {
    std::cout << "Usage:\n"
              << "  Camera/image mode from config: " << app << " [config_yaml_path]\n"
              << "  Local image mode: " << app << " <image_path> <calib_yaml_path>\n";
}

/**
 * @brief 生成 ArUco 位姿程序的默认配置。
 * @return 填充默认值后的配置结构体。
 */
PoseConfig makeDefaultPoseConfig() {
    PoseConfig config;
    config.mode = "image";
    config.imagePath = "./example.jpg";
    config.cameraType = "realsense";
    config.intrinsicsSource = "yaml";
    config.calibYamlPath = "./intrinsic_calib.yaml";
    config.markerSizeM = MARKER_SIZE_M;
    config.dictionary = DEFAULT_DICT_NAME;
    config.cameraWidth = 640;
    config.cameraHeight = 480;
    config.cameraFps = 30;
    return config;
}

/**
 * @brief 从 YAML 文件加载 ArUco 位姿程序配置。
 * @param[in] configPath 配置文件路径。
 * @param[out] config 输出配置结构体。
 * @param[out] error 加载失败时返回错误信息。
 * @return 加载成功返回 true，否则返回 false。
 */
bool loadPoseConfigFromYaml(const std::string& configPath, PoseConfig& config, std::string& error) {
    cv::FileStorage fs(configPath, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        error = "Cannot open pose config yaml: " + configPath;
        return false;
    }
    fs["mode"] >> config.mode;
    fs["image_path"] >> config.imagePath;
    fs["camera_type"] >> config.cameraType;
    fs["intrinsics_source"] >> config.intrinsicsSource;
    fs["calib_yaml_path"] >> config.calibYamlPath;
    fs["marker_size_m"] >> config.markerSizeM;
    fs["dictionary"] >> config.dictionary;
    fs["camera_width"] >> config.cameraWidth;
    fs["camera_height"] >> config.cameraHeight;
    fs["camera_fps"] >> config.cameraFps;

    const std::string baseDir = dirnameOf(configPath);
    if (!isAbsolutePath(config.imagePath)) {
        config.imagePath = joinPath(baseDir, config.imagePath);
    }
    if (!isAbsolutePath(config.calibYamlPath)) {
        config.calibYamlPath = joinPath(baseDir, config.calibYamlPath);
    }
    return true;
}

}  // 命名空间

/**
 * @brief 程序入口，执行 ArUco 图像或实时相机位姿估计。
 * @param[in] argc 命令行参数个数。
 * @param[in] argv 命令行参数数组。
 * @return 成功返回 0，失败返回非 0。
 */
int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        printUsage(argv[0]);
        return 0;
    }
    if (argc != 1 && argc != 2 && argc != 3) {
        printUsage(argv[0]);
        return -1;
    }

    PoseConfig config = makeDefaultPoseConfig();
    if (argc == 3) {
        config.mode = "image";
        config.imagePath = argv[1];
        config.calibYamlPath = argv[2];
    } else {
        const std::string configPath = (argc == 2) ? argv[1] : DEFAULT_CONFIG_PATH;
        std::string error;
        if (!loadPoseConfigFromYaml(configPath, config, error)) {
            std::cerr << "[Error] " << error << std::endl;
            return -1;
        }
    }

    cv::aruco::PREDEFINED_DICTIONARY_NAME dictId;
    if (!resolveDictionary(config.dictionary, dictId)) {
        std::cerr << "[Error] Unsupported default dictionary." << std::endl;
        return -1;
    }
    const cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(dictId);

    if (config.mode == "image") {
        if (config.imagePath.empty() || config.calibYamlPath.empty()) {
            std::cerr << "[Error] image mode requires image_path and calib_yaml_path in yaml config." << std::endl;
            return -1;
        }
        return runLocalImageMode(config.imagePath, config.calibYamlPath, dictionary, config.markerSizeM);
    }

    if (config.mode == "camera") {
        if (config.cameraWidth <= 0 || config.cameraHeight <= 0 || config.cameraFps <= 0) {
            std::cerr << "[Error] camera_width/camera_height/camera_fps must be > 0." << std::endl;
            return -1;
        }
        aruco_camera::IntrinsicsSource intrinsicsSource;
        if (!aruco_camera::parseIntrinsicsSource(config.intrinsicsSource, intrinsicsSource)) {
            std::cerr << "[Error] camera mode intrinsics_source must be sdk or yaml." << std::endl;
            return -1;
        }
        if (config.cameraType != "realsense" && config.cameraType != "orbbec") {
            std::cerr << "[Error] camera_type must be realsense or orbbec." << std::endl;
            return -1;
        }
        if (intrinsicsSource == aruco_camera::IntrinsicsSource::Yaml && config.calibYamlPath.empty()) {
            std::cerr << "[Error] camera mode with yaml intrinsics requires calib_yaml_path." << std::endl;
            return -1;
        }
        const std::string yamlPath = (intrinsicsSource == aruco_camera::IntrinsicsSource::Yaml) ? config.calibYamlPath : "";
        aruco_camera::CameraStreamConfig streamConfig;
        streamConfig.width = config.cameraWidth;
        streamConfig.height = config.cameraHeight;
        streamConfig.fps = config.cameraFps;
        return runCameraMode(config.cameraType, intrinsicsSource, yamlPath, streamConfig, dictionary, config.markerSizeM);
    }

    std::cerr << "[Error] mode must be image or camera in pose yaml config." << std::endl;
    return -1;
}
