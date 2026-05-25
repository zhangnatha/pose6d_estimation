/**
 * @file main.cpp
 * @brief 棋盘格与非对称圆点阵相机内参标定程序
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFDIR) != 0)
#endif
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "CameraBase.h"

namespace {

const std::string DEFAULT_CONFIG_PATH = "./config.yaml";

struct CalibConfig {
    std::string inputMode;       // 输入模式：camera / images
    std::string cameraType;      // 相机类型：realsense / orbbec
    int cameraWidth;
    int cameraHeight;
    int cameraFps;
    std::string boardType;       // 标定板类型：chessboard / acircles
    int boardCols;
    int boardRows;
    double squareSizeMm;         // 相邻角点或圆点中心间距
    int minSamples;
    bool autoCapture;
    int autoCaptureIntervalMs;
    double autoCaptureMinCenterShiftPx;
    double minSharpnessScore;
    std::string imagesGlob;      // images 模式下的输入图片通配符
    std::string rawImagesDir;
    std::string outputYaml;
};

/**
 * @brief 判断路径是否为绝对路径。
 * @param[in] path 待检查路径。
 * @return 绝对路径返回 true，否则返回 false。
 */
bool isAbsolutePath(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/') return true;
    return path.size() > 1 && path[1] == ':';
}

/**
 * @brief 提取路径的目录部分。
 * @param[in] path 输入路径。
 * @return 父目录路径。
 */
std::string dirnameOf(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

/**
 * @brief 拼接基础目录与相对路径。
 * @param[in] base 基础目录。
 * @param[in] rel 相对路径。
 * @return 拼接后的路径字符串。
 */
std::string joinPath(const std::string& base, const std::string& rel) {
    if (base.empty() || base == ".") return rel;
    if (base[base.size() - 1] == '/' || base[base.size() - 1] == '\\') return base + rel;
    return base + "/" + rel;
}

/**
 * @brief 规范化路径分隔符并解析 `.` 与 `..`。
 * @param[in] path 输入路径。
 * @return 规范化后的路径。
 */
std::string normalizePath(const std::string& path) {
    if (path.empty()) return path;

    std::string normalized = path;
    for (size_t i = 0; i < normalized.size(); ++i) {
        if (normalized[i] == '\\') normalized[i] = '/';
    }

    bool isAbs = !normalized.empty() && normalized[0] == '/';
    std::string prefix;
    if (normalized.size() > 1 && normalized[1] == ':') {
        prefix = normalized.substr(0, 2);
        normalized = normalized.substr(2);
        isAbs = !normalized.empty() && normalized[0] == '/';
    }

    std::vector<std::string> parts;
    std::stringstream ss(normalized);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..") {
                parts.pop_back();
            } else if (!isAbs) {
                parts.push_back(part);
            }
            continue;
        }
        parts.push_back(part);
    }

    std::ostringstream out;
    out << prefix;
    if (isAbs) out << "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out << "/";
        out << parts[i];
    }

    const std::string result = out.str();
    if (result.empty()) return isAbs ? "/" : ".";
    return result;
}

/**
 * @brief 将给定路径解析为绝对路径。
 * @param[in] path 输入路径。
 * @return 绝对路径字符串。
 */
std::string absolutePath(const std::string& path) {
    if (path.empty()) return path;
    if (isAbsolutePath(path)) return normalizePath(path);

    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) return normalizePath(path);
    return normalizePath(joinPath(cwd, path));
}

/**
 * @brief 生成当前时间戳字符串。
 * @return 形如 `YYYYMMDD_HHMMSS` 的时间戳。
 */
std::string timestampString() {
    char buf[32];
    std::time_t now = std::time(NULL);
    std::tm tm_now;
#ifdef _WIN32
    localtime_s(&tm_now, &now);
#else
    localtime_r(&now, &tm_now);
#endif
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_now);
    return std::string(buf);
}

/**
 * @brief 若目录不存在则创建目录。
 * @param[in] path 目录路径。
 * @return 目录可用返回 true，否则返回 false。
 */
bool createDirectoryIfNeeded(const std::string& path) {
    if (path.empty() || path == ".") return true;
    struct stat st = {0};
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    #ifdef _WIN32
    if (_mkdir(path.c_str()) == 0) return true;
#else
    if (mkdir(path.c_str(), 0777) == 0) return true;
#endif
    return false;
}

/**
 * @brief 判断文件或目录是否存在。
 * @param[in] path 待检查路径。
 * @return 存在返回 true，否则返回 false。
 */
bool pathExists(const std::string& path) {
    struct stat st = {0};
    return stat(path.c_str(), &st) == 0;
}

/**
 * @brief 确保目标文件的父目录存在。
 * @param[in] filePath 目标文件路径。
 * @return 成功返回 true，否则返回 false。
 */
bool ensureParentDirectory(const std::string& filePath) {
    const std::string dir = dirnameOf(filePath);
    if (dir.empty() || dir == "." || dir == "/") return true;
    struct stat st = {0};
    if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    const std::string parent = dirnameOf(dir);
    if (parent != dir && !ensureParentDirectory(dir)) return false;
    return createDirectoryIfNeeded(dir);
}

/**
 * @brief 结合运行目录和默认规则解析配置文件路径。
 * @param[in] requestedPath 用户请求的路径。
 * @param[in] argv0 程序启动路径。
 * @return 解析后的配置文件绝对路径。
 */
std::string resolveConfigPath(const std::string& requestedPath, const char* argv0) {
    if (requestedPath.empty()) {
        return requestedPath;
    }
    if (isAbsolutePath(requestedPath) || pathExists(requestedPath)) {
        return absolutePath(requestedPath);
    }

    if (argv0 != NULL && argv0[0] != '\0') {
        const std::string candidate = joinPath(dirnameOf(argv0), requestedPath);
        if (pathExists(candidate)) {
            return absolutePath(candidate);
        }
    }

    if (requestedPath == DEFAULT_CONFIG_PATH) {
        const std::string buildRootCandidate = "./config.yaml";
        if (pathExists(buildRootCandidate)) {
            return absolutePath(buildRootCandidate);
        }

        const std::string buildSubdirCandidate = "./config.yaml";
        if (pathExists(buildSubdirCandidate)) {
            return absolutePath(buildSubdirCandidate);
        }
    }

    return absolutePath(requestedPath);
}

/**
 * @brief 解析当前可执行文件所在目录。
 * @param[in] argv0 程序启动路径。
 * @return 可执行文件所在目录。
 */
std::string resolveExecutableDir(const char* argv0) {
    if (argv0 == NULL || argv0[0] == '\0') {
        return absolutePath(".");
    }
    return dirnameOf(resolveConfigPath(argv0, argv0));
}

/**
 * @brief 生成默认标定配置。
 * @return 默认配置结构体。
 */
CalibConfig makeDefaultConfig() {
    CalibConfig c;
    c.inputMode = "camera";
    c.cameraType = "realsense";
    c.cameraWidth = 1280;
    c.cameraHeight = 720;
    c.cameraFps = 30;
    c.boardType = "chessboard";
    c.boardCols = 7;
    c.boardRows = 6;
    c.squareSizeMm = 25.0;
    c.minSamples = 20;
    c.autoCapture = true;
    c.autoCaptureIntervalMs = 500;
    c.autoCaptureMinCenterShiftPx = 25.0;
    c.minSharpnessScore = 120.0;
    c.imagesGlob = "assert/raw_*.png";
    c.rawImagesDir = "assert";
    c.outputYaml = "intrinsic_calib.yaml";
    return c;
}

/**
 * @brief 从 YAML 文件加载标定配置。
 * @param[in] configPath 配置文件路径。
 * @param[in] executableDir 可执行文件目录。
 * @param[out] c 输出配置结构体。
 * @param[out] error 加载失败时返回错误信息。
 * @return 加载成功返回 true，否则返回 false。
 */
bool loadConfig(const std::string& configPath,
                const std::string& executableDir,
                CalibConfig& c,
                std::string& error) {
    cv::FileStorage fs(configPath, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        error = "Cannot open calib config yaml: " + configPath;
        return false;
    }

    fs["input_mode"] >> c.inputMode;
    fs["camera_type"] >> c.cameraType;
    fs["camera_width"] >> c.cameraWidth;
    fs["camera_height"] >> c.cameraHeight;
    fs["camera_fps"] >> c.cameraFps;
    fs["board_type"] >> c.boardType;
    fs["board_cols"] >> c.boardCols;
    fs["board_rows"] >> c.boardRows;
    fs["square_size_mm"] >> c.squareSizeMm;
    fs["min_samples"] >> c.minSamples;
    fs["auto_capture"] >> c.autoCapture;
    fs["auto_capture_interval_ms"] >> c.autoCaptureIntervalMs;
    fs["auto_capture_min_center_shift_px"] >> c.autoCaptureMinCenterShiftPx;
    fs["min_sharpness_score"] >> c.minSharpnessScore;
    fs["images_glob"] >> c.imagesGlob;
    fs["output_yaml"] >> c.outputYaml;

    const std::string baseDir = dirnameOf(absolutePath(configPath));
    c.rawImagesDir = normalizePath(joinPath(baseDir, "assert"));
    if (!isAbsolutePath(c.imagesGlob)) c.imagesGlob = normalizePath(joinPath(baseDir, c.imagesGlob));
    else c.imagesGlob = normalizePath(c.imagesGlob);
    if (!isAbsolutePath(c.outputYaml)) c.outputYaml = normalizePath(joinPath(executableDir, c.outputYaml));
    else c.outputYaml = normalizePath(c.outputYaml);
    return true;
}

/**
 * @brief 根据标定板配置生成对应的三维角点坐标。
 * @param[in] c 标定配置。
 * @return 标定板三维点列表。
 */
std::vector<cv::Point3f> makeBoardObjectPoints(const CalibConfig& c) {
    std::vector<cv::Point3f> pts;
    const float spacing = static_cast<float>(c.squareSizeMm);
    if (c.boardType == "chessboard") {
        for (int r = 0; r < c.boardRows; ++r) {
            for (int col = 0; col < c.boardCols; ++col) {
                pts.push_back(cv::Point3f(col * spacing, r * spacing, 0.0f));
            }
        }
    } else {  // 非对称圆点阵
        for (int r = 0; r < c.boardRows; ++r) {
            for (int col = 0; col < c.boardCols; ++col) {
                const float x = static_cast<float>((2 * col + (r % 2)) * spacing);
                const float y = static_cast<float>(r * spacing);
                pts.push_back(cv::Point3f(x, y, 0.0f));
            }
        }
    }
    return pts;
}

/**
 * @brief 在灰度图中检测棋盘格或非对称圆点阵角点。
 * @param[in] gray 输入灰度图。
 * @param[in] c 标定配置。
 * @param[out] corners 检测到的角点。
 * @return 检测成功返回 true，否则返回 false。
 */
bool detectBoard(const cv::Mat& gray, const CalibConfig& c, std::vector<cv::Point2f>& corners) {
    const cv::Size patternSize(c.boardCols, c.boardRows);
    if (c.boardType == "chessboard") {
        const int flags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
        bool found = cv::findChessboardCorners(gray, patternSize, corners, flags);
        if (found) {
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));
        }
        return found;
    }
    if (c.boardType == "acircles") {
        return cv::findCirclesGrid(gray, patternSize, corners, cv::CALIB_CB_ASYMMETRIC_GRID);
    }
    return false;
}

/**
 * @brief 计算角点区域的清晰度评分。
 * @param[in] gray 输入灰度图。
 * @param[in] corners 检测到的角点集合。
 * @return 基于拉普拉斯方差的清晰度评分。
 */
double computeCornerSharpnessScore(const cv::Mat& gray, const std::vector<cv::Point2f>& corners) {
    if (gray.empty() || corners.empty()) {
        return 0.0;
    }

    cv::Rect roi = cv::boundingRect(corners);
    const int expand = 12;
    roi.x = std::max(0, roi.x - expand);
    roi.y = std::max(0, roi.y - expand);
    roi.width = std::min(gray.cols - roi.x, roi.width + expand * 2);
    roi.height = std::min(gray.rows - roi.y, roi.height + expand * 2);
    if (roi.width <= 0 || roi.height <= 0) {
        return 0.0;
    }

    cv::Mat laplacian;
    cv::Laplacian(gray(roi), laplacian, CV_64F);
    cv::Scalar meanValue;
    cv::Scalar stddevValue;
    cv::meanStdDev(laplacian, meanValue, stddevValue);
    return stddevValue[0] * stddevValue[0];
}

/**
 * @brief 生成单张采样图像的保存路径。
 * @param[in] dir 输出目录。
 * @param[in] prefix 文件名前缀。
 * @param[in] index 样本编号。
 * @return 生成后的文件路径。
 */
std::string makeSampleImagePath(const std::string& dir, const std::string& prefix, int index) {
    std::ostringstream oss;
    oss << prefix << "_" << std::setfill('0') << std::setw(3) << index << ".png";
    return joinPath(dir, oss.str());
}

/**
 * @brief 保存原始样本图与预览图。
 * @param[in] rawDir 原图保存目录。
 * @param[in] previewDir 预览图保存目录。
 * @param[in] sessionTag 当前采样会话标签。
 * @param[in] sampleIndex 样本编号。
 * @param[in] rawImage 原始图像。
 * @param[in] previewImage 预览图像。
 * @return 保存成功返回 true，否则返回 false。
 */
bool saveSampleArtifacts(const std::string& rawDir,
                         const std::string& previewDir,
                         const std::string& sessionTag,
                         int sampleIndex,
                         const cv::Mat& rawImage,
                         const cv::Mat& previewImage) {
    const std::string rawPath = makeSampleImagePath(rawDir, "raw_" + sessionTag, sampleIndex);
    const std::string previewPath = makeSampleImagePath(previewDir, "preview", sampleIndex);
    const bool rawOk = cv::imwrite(rawPath, rawImage);
    const bool previewOk = cv::imwrite(previewPath, previewImage);
    if (!rawOk || !previewOk) {
        std::cerr << "[Warn] Failed to save sample artifacts for sample " << sampleIndex << std::endl;
        return false;
    }

    std::cout << "[Info] Raw saved: " << rawPath << std::endl;
    std::cout << "[Info] Preview saved: " << previewPath << std::endl;
    return true;
}

/**
 * @brief 从实时相机采集标定样本。
 * @param[in] c 标定配置。
 * @param[out] imagePoints 输出图像角点集合。
 * @param[out] imageSize 输出图像尺寸。
 * @return 采集成功返回 true，否则返回 false。
 */
bool runCameraCollect(const CalibConfig& c,
                      std::vector<std::vector<cv::Point2f> >& imagePoints,
                      cv::Size& imageSize) {
#ifndef _WIN32
    // On non-Windows systems, check for a graphical display environment
    if ((std::getenv("DISPLAY") == NULL || std::string(std::getenv("DISPLAY")).empty()) &&
        (std::getenv("WAYLAND_DISPLAY") == NULL || std::string(std::getenv("WAYLAND_DISPLAY")).empty())) {
        std::cerr << "[Error] Camera calibration mode requires GUI display." << std::endl;
        return false;
    }
#endif

    std::unique_ptr<aruco_camera::CameraBase> camera = aruco_camera::createCamera(c.cameraType);
    if (!camera) {
        std::cerr << "[Error] Unsupported camera_type: " << c.cameraType << std::endl;
        return false;
    }

    std::string error;
    aruco_camera::CameraStreamConfig streamConfig;
    streamConfig.width = c.cameraWidth;
    streamConfig.height = c.cameraHeight;
    streamConfig.fps = c.cameraFps;
    if (!camera->start(streamConfig, error)) {
        std::cerr << "[Error] Start camera failed: " << error << std::endl;
        return false;
    }

    const std::string sessionTag = timestampString();
    const std::string previewDir = joinPath(dirnameOf(c.outputYaml), "preview_" + sessionTag);
    if (!createDirectoryIfNeeded(previewDir)) {
        std::cerr << "[Error] Cannot create preview directory: " << previewDir << std::endl;
        camera->stop();
        return false;
    }
    if (!ensureParentDirectory(joinPath(c.rawImagesDir, "placeholder.txt")) ||
        !createDirectoryIfNeeded(c.rawImagesDir)) {
        std::cerr << "[Error] Cannot create raw image directory: " << c.rawImagesDir << std::endl;
        camera->stop();
        return false;
    }

    try {
        cv::namedWindow("Calib Collect", cv::WINDOW_AUTOSIZE);
    } catch (const cv::Exception&) {
        std::cerr << "[Error] Failed to initialize GUI backend for calibration." << std::endl;
        camera->stop();
        return false;
    }
    std::cout << "[Info] Press 'c' to capture current frame when corners are detected." << std::endl;
    std::cout << "[Info] Press 'Enter' to calibrate, 'Esc' to exit." << std::endl;
    if (c.autoCapture) {
        std::cout << "[Info] Auto capture enabled: interval=" << c.autoCaptureIntervalMs
                  << "ms, min_center_shift=" << c.autoCaptureMinCenterShiftPx << "px" << std::endl;
    }

    std::clock_t lastCaptureTick = 0;
    bool hasLastCenter = false;
    cv::Point2f lastCenter(0.0f, 0.0f);

    while (true) {
        cv::Mat frame;
        if (!camera->grabFrame(frame, error)) {
            std::cerr << "[Error] Grab frame failed: " << error << std::endl;
            camera->stop();
            return false;
        }
        imageSize = frame.size();

        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        std::vector<cv::Point2f> corners;
        const bool found = detectBoard(gray, c, corners);
        const double sharpnessScore = found ? computeCornerSharpnessScore(gray, corners) : 0.0;
        const bool sharpEnough = !found || c.minSharpnessScore <= 0.0 || sharpnessScore >= c.minSharpnessScore;

        cv::Mat view = frame.clone();
        if (found) {
            cv::drawChessboardCorners(view, cv::Size(c.boardCols, c.boardRows), corners, found);
        }

        std::ostringstream oss;
        oss << "samples: " << imagePoints.size() << "/" << c.minSamples
            << " found:" << (found ? "yes" : "no")
            << " sharp:" << std::fixed << std::setprecision(1) << sharpnessScore;
        cv::rectangle(view, cv::Rect(0, 0, 520, 54), cv::Scalar(0, 0, 0), -1);
        cv::putText(view, oss.str(), cv::Point(10, 22), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
        cv::putText(view,
                    sharpEnough ? "quality: pass" : "quality: blur filtered",
                    cv::Point(10, 46),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    sharpEnough ? cv::Scalar(80, 220, 80) : cv::Scalar(60, 90, 255),
                    1);
        try {
            cv::imshow("Calib Collect", view);
        } catch (const cv::Exception&) {
            std::cerr << "[Error] Failed to render calibration window." << std::endl;
            camera->stop();
            return false;
        }

        int key = -1;
        try {
            key = cv::waitKey(1);
        } catch (const cv::Exception&) {
            std::cerr << "[Error] Failed to process GUI events." << std::endl;
            camera->stop();
            return false;
        }
        if (key == 27) {
            camera->stop();
            return false;
        }
        bool shouldCapture = (key == 'c' || key == 'C') && found && sharpEnough;
        if ((key == 'c' || key == 'C') && found && !sharpEnough) {
            std::cout << "[Warn] Skip manual capture: blur score " << sharpnessScore
                      << " < threshold " << c.minSharpnessScore << std::endl;
        }
        if (c.autoCapture && found) {
            cv::Point2f center(0.0f, 0.0f);
            for (size_t i = 0; i < corners.size(); ++i) {
                center += corners[i];
            }
            center *= (1.0f / static_cast<float>(corners.size()));
            const std::clock_t nowTick = std::clock();
            const double elapsedMs = (lastCaptureTick == 0)
                                         ? 1e9
                                         : (static_cast<double>(nowTick - lastCaptureTick) * 1000.0 / CLOCKS_PER_SEC);
            const double dx = static_cast<double>(center.x - lastCenter.x);
            const double dy = static_cast<double>(center.y - lastCenter.y);
            const double shift = std::sqrt(dx * dx + dy * dy);
            if (elapsedMs >= c.autoCaptureIntervalMs &&
                (!hasLastCenter || shift >= c.autoCaptureMinCenterShiftPx) &&
                sharpEnough) {
                shouldCapture = true;
            }
        }

        if (shouldCapture) {
            imagePoints.push_back(corners);
            saveSampleArtifacts(c.rawImagesDir, previewDir, sessionTag,
                                static_cast<int>(imagePoints.size()), frame, view);
            cv::Point2f center(0.0f, 0.0f);
            for (size_t i = 0; i < corners.size(); ++i) {
                center += corners[i];
            }
            center *= (1.0f / static_cast<float>(corners.size()));
            lastCenter = center;
            hasLastCenter = true;
            lastCaptureTick = std::clock();
            std::cout << "[Info] Captured sample " << imagePoints.size() << std::endl;
        }
        if (key == 13 || key == 10) {
            if (static_cast<int>(imagePoints.size()) < c.minSamples) {
                std::cout << "[Warn] Need at least " << c.minSamples << " samples." << std::endl;
                continue;
            }
            break;
        }
    }

    camera->stop();
    return true;
}

/**
 * @brief 从本地图像批量提取标定样本。
 * @param[in] c 标定配置。
 * @param[out] imagePoints 输出图像角点集合。
 * @param[out] imageSize 输出图像尺寸。
 * @return 满足最少样本数返回 true，否则返回 false。
 */
bool runImagesCollect(const CalibConfig& c,
                      std::vector<std::vector<cv::Point2f> >& imagePoints,
                      cv::Size& imageSize) {
    std::vector<cv::String> files;
    cv::glob(c.imagesGlob, files, false);
    if (files.empty()) {
        std::cerr << "[Error] No image matched: " << c.imagesGlob << std::endl;
        return false;
    }

    int used = 0;
    const std::string sessionTag = timestampString();
    const std::string previewDir = joinPath(dirnameOf(c.outputYaml), "preview_" + sessionTag);
    if (!createDirectoryIfNeeded(previewDir)) {
        std::cerr << "[Error] Cannot create preview directory: " << previewDir << std::endl;
        return false;
    }
    if (!ensureParentDirectory(joinPath(c.rawImagesDir, "placeholder.txt")) ||
        !createDirectoryIfNeeded(c.rawImagesDir)) {
        std::cerr << "[Error] Cannot create raw image directory: " << c.rawImagesDir << std::endl;
        return false;
    }
    for (size_t i = 0; i < files.size(); ++i) {
        cv::Mat img = cv::imread(files[i]);
        if (img.empty()) continue;
        imageSize = img.size();
        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        std::vector<cv::Point2f> corners;
        if (detectBoard(gray, c, corners)) {
            const double sharpnessScore = computeCornerSharpnessScore(gray, corners);
            if (c.minSharpnessScore > 0.0 && sharpnessScore < c.minSharpnessScore) {
                std::cout << "[Warn] Skip blurry sample: " << files[i]
                          << ", sharpness=" << sharpnessScore
                          << ", threshold=" << c.minSharpnessScore << std::endl;
                continue;
            }
            imagePoints.push_back(corners);
            cv::Mat view = img.clone();
            if (c.boardType == "chessboard") {
                cv::drawChessboardCorners(view, cv::Size(c.boardCols, c.boardRows), corners, true);
            }
            saveSampleArtifacts(c.rawImagesDir, previewDir, sessionTag, used + 1, img, view);
            ++used;
        }
    }
    std::cout << "[Info] Input images: " << files.size() << ", valid detections: " << used << std::endl;
    return used >= c.minSamples;
}

/**
 * @brief 执行相机标定并将结果写入 YAML 文件。
 * @param[in] c 标定配置。
 * @param[in] imagePoints 图像角点集合。
 * @param[in] imageSize 图像尺寸。
 * @return 标定并保存成功返回 true，否则返回 false。
 */
bool calibrateAndSave(const CalibConfig& c,
                      const std::vector<std::vector<cv::Point2f> >& imagePoints,
                      const cv::Size& imageSize) {
    std::vector<std::vector<cv::Point3f> > objectPoints;
    const std::vector<cv::Point3f> board = makeBoardObjectPoints(c);
    objectPoints.resize(imagePoints.size(), board);

    cv::Mat K = cv::Mat::eye(3, 3, CV_64F);
    cv::Mat D = cv::Mat::zeros(8, 1, CV_64F);
    std::vector<cv::Mat> rvecs, tvecs;
    const int flags = 0;
    const cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 100, 1e-6);
    const double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize, K, D, rvecs, tvecs, flags, criteria);

    if (!ensureParentDirectory(c.outputYaml)) {
        std::cerr << "[Error] Cannot create output directory for: " << c.outputYaml << std::endl;
        return false;
    }
    cv::FileStorage fs(c.outputYaml, cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        std::cerr << "[Error] Cannot write calibration yaml: " << c.outputYaml << std::endl;
        return false;
    }
    fs << "CameraMatrix" << K;
    fs << "DistCoeffs" << D;
    fs << "ReprojectionError" << rms;
    fs << "ImageWidth" << imageSize.width;
    fs << "ImageHeight" << imageSize.height;
    fs << "BoardType" << c.boardType;
    fs << "BoardCols" << c.boardCols;
    fs << "BoardRows" << c.boardRows;
    fs << "SquareSizeMm" << c.squareSizeMm;
    fs << "Samples" << static_cast<int>(imagePoints.size());
    fs.release();

    std::cout << "[Info] Calibration finished, RMS: " << rms << std::endl;
    std::cout << "[Info] Saved yaml: " << c.outputYaml << std::endl;
    return true;
}

/**
 * @brief 打印命令行使用说明。
 * @param[in] app 可执行程序名称。
 */
void printUsage(const char* app) {
    std::cout << "Usage:\n"
              << "  " << app << " [config_yaml_path]\n";
}

}  // 命名空间

/**
 * @brief 程序入口，执行相机标定配置加载、采样与标定结果输出。
 * @param[in] argc 命令行参数个数。
 * @param[in] argv 命令行参数数组。
 * @return 成功返回 0，失败返回非 0。
 */
int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        printUsage(argv[0]);
        return 0;
    }
    if (argc > 2) {
        printUsage(argv[0]);
        return -1;
    }

    const std::string configPath = resolveConfigPath((argc == 2) ? argv[1] : DEFAULT_CONFIG_PATH, argv[0]);
    const std::string executableDir = resolveExecutableDir(argv[0]);
    CalibConfig config = makeDefaultConfig();
    std::string error;
    if (!loadConfig(configPath, executableDir, config, error)) {
        std::cerr << "[Error] " << error << std::endl;
        return -1;
    }

    if (config.boardType != "chessboard" && config.boardType != "acircles") {
        std::cerr << "[Error] board_type must be chessboard or acircles." << std::endl;
        return -1;
    }
    if (config.boardCols <= 0 || config.boardRows <= 0 || config.squareSizeMm <= 0.0 || config.minSamples <= 0) {
        std::cerr << "[Error] Invalid board config values." << std::endl;
        return -1;
    }
    if (config.minSharpnessScore < 0.0) {
        std::cerr << "[Error] min_sharpness_score must be >= 0." << std::endl;
        return -1;
    }

    std::vector<std::vector<cv::Point2f> > imagePoints;
    cv::Size imageSize;
    bool ok = false;
    if (config.inputMode == "camera") {
        ok = runCameraCollect(config, imagePoints, imageSize);
    } else if (config.inputMode == "images") {
        ok = runImagesCollect(config, imagePoints, imageSize);
    } else {
        std::cerr << "[Error] input_mode must be camera or images." << std::endl;
        return -1;
    }

    if (!ok) {
        std::cerr << "[Error] Failed to collect enough valid samples." << std::endl;
        return -1;
    }

    if (!calibrateAndSave(config, imagePoints, imageSize)) {
        return -1;
    }
    return 0;
}
