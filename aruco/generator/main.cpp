/**
 * @file main.cpp
 * @brief 基于 OpenCV 生成可打印的 ArUco 标记图
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

#include <opencv2/aruco.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

const std::string DEFAULT_DICT_NAME = "DICT_4X4_50";
const int DEFAULT_START_ID = 0;
const int DEFAULT_COUNT = 10;
const double DEFAULT_MARKER_SIZE_MM = 40.0;
const int DEFAULT_BORDER_BITS = 1;
const int DEFAULT_DPI = 300;
const double A4_WIDTH_MM = 210.0;
const double A4_HEIGHT_MM = 297.0;
const double PAGE_MARGIN_MM = 10.0;
const double LABEL_HEIGHT_MM = 12.0;
const double CELL_GAP_MM = 6.0;
const std::string DEFAULT_LAYOUT_MODE = "grid";
const std::string DEFAULT_ORIENTATION = "portrait";
const std::string DEFAULT_CONFIG_PATH = "./config.yaml";

struct LayoutConfig {
    std::string dictName;
    int startId;
    int count;
    double markerSizeMm;
    int dpi;
    int borderBits;
    std::string layoutMode;
    std::string orientation;
};

struct PageGeometry {
    int pageWidthPx;
    int pageHeightPx;
    int marginPx;
    int gapPx;
    int labelHeightPx;
    int markerSizePx;
    int cellWidthPx;
    int cellHeightPx;
    int columns;
    int rows;
    int markersPerPage;
    double actualMarkerSizeMm;
};

/**
 * @brief 判断当前排版模式是否为单标记居中模式。
 * @param[in] layoutMode 排版模式字符串。
 * @return 单标记模式返回 true，否则返回 false。
 */
bool isSingleMode(const std::string& layoutMode) {
    return layoutMode == "single";
}

/**
 * @brief 若目录不存在则创建输出目录。
 * @param[in] path 待创建目录路径。
 * @return 目录可用返回 true，否则返回 false。
 */
bool createDirectoryIfNeeded(const std::string& path) {
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0 || errno == EEXIST) {
        return true;
    }
#else
    if (mkdir(path.c_str(), 0777) == 0 || errno == EEXIST) {
        return true;
    }
#endif
    std::cerr << "[Error] Failed to create directory: " << path
              << ", reason: " << std::strerror(errno) << std::endl;
    return false;
}

/**
 * @brief 将字典名称解析为 OpenCV ArUco 字典及其容量。
 * @param[in] name 字典名称。
 * @param[out] dictId 输出的字典枚举。
 * @param[out] dictSize 输出的字典容量。
 * @return 解析成功返回 true，否则返回 false。
 */
bool resolveDictionary(const std::string& name,
                       cv::aruco::PREDEFINED_DICTIONARY_NAME& dictId,
                       int& dictSize) {
    static const std::map<std::string, std::pair<cv::aruco::PREDEFINED_DICTIONARY_NAME, int> > kDictMap = {
        {"DICT_4X4_50", {cv::aruco::DICT_4X4_50, 50}},
        {"DICT_4X4_100", {cv::aruco::DICT_4X4_100, 100}},
        {"DICT_4X4_250", {cv::aruco::DICT_4X4_250, 250}},
        {"DICT_4X4_1000", {cv::aruco::DICT_4X4_1000, 1000}},
        {"DICT_5X5_50", {cv::aruco::DICT_5X5_50, 50}},
        {"DICT_5X5_100", {cv::aruco::DICT_5X5_100, 100}},
        {"DICT_5X5_250", {cv::aruco::DICT_5X5_250, 250}},
        {"DICT_5X5_1000", {cv::aruco::DICT_5X5_1000, 1000}},
        {"DICT_6X6_50", {cv::aruco::DICT_6X6_50, 50}},
        {"DICT_6X6_100", {cv::aruco::DICT_6X6_100, 100}},
        {"DICT_6X6_250", {cv::aruco::DICT_6X6_250, 250}},
        {"DICT_6X6_1000", {cv::aruco::DICT_6X6_1000, 1000}},
        {"DICT_7X7_50", {cv::aruco::DICT_7X7_50, 50}},
        {"DICT_7X7_100", {cv::aruco::DICT_7X7_100, 100}},
        {"DICT_7X7_250", {cv::aruco::DICT_7X7_250, 250}},
        {"DICT_7X7_1000", {cv::aruco::DICT_7X7_1000, 1000}},
        {"DICT_ARUCO_ORIGINAL", {cv::aruco::DICT_ARUCO_ORIGINAL, 1024}}
    };

    std::map<std::string, std::pair<cv::aruco::PREDEFINED_DICTIONARY_NAME, int> >::const_iterator it =
        kDictMap.find(name);
    if (it == kDictMap.end()) {
        return false;
    }

    dictId = it->second.first;
    dictSize = it->second.second;
    return true;
}

/**
 * @brief 生成单个带文字标签的标记画布。
 * @param[in] dictionary ArUco 字典对象。
 * @param[in] markerId 标记编号。
 * @param[in] markerSizePx 标记边长，单位像素。
 * @param[in] borderBits 外边框比特宽度。
 * @param[in] labelHeightPx 文字区域高度，单位像素。
 * @param[in] cellWidthPx 单元格宽度，单位像素。
 * @param[in] cellHeightPx 单元格高度，单位像素。
 * @param[in] dictName 字典名称。
 * @param[in] markerSizeMm 标记实际尺寸，单位毫米。
 * @return 生成后的灰度画布。
 */
cv::Mat createMarkerCanvas(const cv::Ptr<cv::aruco::Dictionary>& dictionary,
                           int markerId,
                           int markerSizePx,
                           int borderBits,
                           int labelHeightPx,
                           int cellWidthPx,
                           int cellHeightPx,
                           const std::string& dictName,
                           double markerSizeMm) {
    cv::Mat marker;
    cv::aruco::drawMarker(dictionary, markerId, markerSizePx, marker, borderBits);

    cv::Mat canvas(cellHeightPx, cellWidthPx, CV_8UC1, cv::Scalar(255));
    const int left = (cellWidthPx - markerSizePx) / 2;
    marker.copyTo(canvas(cv::Rect(left, 0, marker.cols, marker.rows)));

    std::ostringstream oss;
    oss << dictName << "  ID:" << markerId << "  " << std::fixed << std::setprecision(1) << markerSizeMm << "mm";
    cv::putText(canvas, oss.str(), cv::Point(16, markerSizePx + std::max(24, labelHeightPx / 2)),
                cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0), 2);

    return canvas;
}

/**
 * @brief 将毫米转换为像素。
 * @param[in] mm 长度，单位毫米。
 * @param[in] dpi 输出分辨率。
 * @return 对应的像素值。
 */
int mmToPx(double mm, int dpi) {
    return static_cast<int>(std::lround(mm / 25.4 * static_cast<double>(dpi)));
}

/**
 * @brief 将像素转换为毫米。
 * @param[in] px 长度，单位像素。
 * @param[in] dpi 输出分辨率。
 * @return 对应的毫米值。
 */
double pxToMm(int px, int dpi) {
    return static_cast<double>(px) * 25.4 / static_cast<double>(dpi);
}

/**
 * @brief 根据请求尺寸、DPI 和排版模式构建整页几何参数。
 * @param[in] requestedMarkerSizeMm 请求的标记尺寸，单位毫米。
 * @param[in] dpi 输出分辨率。
 * @param[in] layoutMode 排版模式。
 * @param[in] orientation 纸张方向。
 * @return 计算后的页面几何参数。
 */
PageGeometry buildPageGeometry(double requestedMarkerSizeMm,
                               int dpi,
                               const std::string& layoutMode,
                               const std::string& orientation) {
    PageGeometry g;
    const bool landscape = orientation == "landscape";
    g.pageWidthPx = mmToPx(landscape ? A4_HEIGHT_MM : A4_WIDTH_MM, dpi);
    g.pageHeightPx = mmToPx(landscape ? A4_WIDTH_MM : A4_HEIGHT_MM, dpi);
    g.marginPx = mmToPx(PAGE_MARGIN_MM, dpi);
    g.gapPx = mmToPx(CELL_GAP_MM, dpi);
    g.labelHeightPx = mmToPx(LABEL_HEIGHT_MM, dpi);
    g.markerSizePx = mmToPx(requestedMarkerSizeMm, dpi);
    g.cellWidthPx = g.markerSizePx;
    g.cellHeightPx = g.markerSizePx + g.labelHeightPx;

    const int usableWidth = g.pageWidthPx - 2 * g.marginPx;
    const int usableHeight = g.pageHeightPx - 2 * g.marginPx;
    if (isSingleMode(layoutMode)) {
        g.columns = 1;
        g.rows = 1;
    } else {
        g.columns = std::max(1, (usableWidth + g.gapPx) / (g.cellWidthPx + g.gapPx));
        g.rows = std::max(1, (usableHeight + g.gapPx) / (g.cellHeightPx + g.gapPx));
    }
    g.markersPerPage = g.columns * g.rows;
    g.actualMarkerSizeMm = pxToMm(g.markerSizePx, dpi);
    return g;
}

/**
 * @brief 输出排版信息清单文件。
 * @param[in] outputDir 输出目录。
 * @param[in] config 布局配置。
 * @param[in] geometry 页面几何参数。
 * @param[in] totalPages 总页数。
 * @return 保存成功返回 true，否则返回 false。
 */
bool saveManifest(const std::string& outputDir,
                  const LayoutConfig& config,
                  const PageGeometry& geometry,
                  int totalPages) {
    const std::string manifestPath = outputDir + "/layout_info.txt";
    std::ofstream out(manifestPath.c_str());
    if (!out.is_open()) {
        std::cerr << "[Error] Failed to write manifest: " << manifestPath << std::endl;
        return false;
    }

    out << "Dictionary: " << config.dictName << "\n";
    out << "Marker ID range: " << config.startId << " - " << (config.startId + config.count - 1) << "\n";
    out << "Requested marker size (mm): " << std::fixed << std::setprecision(3) << config.markerSizeMm << "\n";
    out << "Actual marker size on A4 (mm): " << geometry.actualMarkerSizeMm << "\n";
    out << "DPI: " << config.dpi << "\n";
    out << "Layout mode: " << config.layoutMode << "\n";
    out << "Orientation: " << config.orientation << "\n";
    out << "A4 size (mm): " << A4_WIDTH_MM << " x " << A4_HEIGHT_MM << "\n";
    out << "A4 size (px): " << geometry.pageWidthPx << " x " << geometry.pageHeightPx << "\n";
    out << "Page margin (mm): " << PAGE_MARGIN_MM << "\n";
    out << "Grid: " << geometry.columns << " columns x " << geometry.rows << " rows\n";
    out << "Markers per page: " << geometry.markersPerPage << "\n";
    out << "Total pages: " << totalPages << "\n";
    return true;
}

/**
 * @brief 生成默认 ArUco 生成配置。
 * @return 默认配置结构体。
 */
LayoutConfig makeDefaultLayoutConfig() {
    LayoutConfig config;
    config.dictName = DEFAULT_DICT_NAME;
    config.startId = DEFAULT_START_ID;
    config.count = DEFAULT_COUNT;
    config.markerSizeMm = DEFAULT_MARKER_SIZE_MM;
    config.dpi = DEFAULT_DPI;
    config.borderBits = DEFAULT_BORDER_BITS;
    config.layoutMode = DEFAULT_LAYOUT_MODE;
    config.orientation = DEFAULT_ORIENTATION;
    return config;
}

/**
 * @brief 从 YAML 文件读取标记生成配置。
 * @param[in] configPath 配置文件路径。
 * @param[out] config 输出配置结构体。
 * @param[out] error 加载失败时返回错误信息。
 * @return 加载成功返回 true，否则返回 false。
 */
bool loadLayoutConfigFromYaml(const std::string& configPath, LayoutConfig& config, std::string& error) {
    cv::FileStorage fs(configPath, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        error = "Cannot open generator config yaml: " + configPath;
        return false;
    }

    fs["dictionary"] >> config.dictName;
    fs["start_id"] >> config.startId;
    fs["count"] >> config.count;
    fs["marker_size_mm"] >> config.markerSizeMm;
    fs["dpi"] >> config.dpi;
    fs["border_bits"] >> config.borderBits;
    fs["layout_mode"] >> config.layoutMode;
    fs["orientation"] >> config.orientation;
    return true;
}

/**
 * @brief 打印命令行使用说明。
 * @param[in] app 可执行程序名称。
 */
void printUsage(const char* app) {
    std::cout << "Usage:\n"
              << "  " << app << " [config_yaml_path]\n\n"
              << "Example:\n"
              << "  " << app << "\n"
              << "  " << app << " ../../../aruco/generator/config.yaml\n";
}

}  // 命名空间

/**
 * @brief 程序入口，执行 ArUco 标记生成、排版与清单输出。
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

    const std::string configPath = (argc == 2) ? argv[1] : DEFAULT_CONFIG_PATH;
    LayoutConfig config = makeDefaultLayoutConfig();
    std::string configError;
    if (!loadLayoutConfigFromYaml(configPath, config, configError)) {
        std::cerr << "[Error] " << configError << std::endl;
        return -1;
    }

    cv::aruco::PREDEFINED_DICTIONARY_NAME dictId;
    int dictSize = 0;
    if (!resolveDictionary(config.dictName, dictId, dictSize)) {
        std::cerr << "[Error] Unsupported dictionary: " << config.dictName << std::endl;
        printUsage(argv[0]);
        return -1;
    }

    if (config.startId < 0 || config.count <= 0 || config.markerSizeMm <= 0.0 ||
        config.dpi <= 0 || config.borderBits <= 0) {
        std::cerr << "[Error] Invalid arguments." << std::endl;
        return -1;
    }
    if (config.layoutMode != "grid" && config.layoutMode != "single") {
        std::cerr << "[Error] layout_mode must be 'grid' or 'single'." << std::endl;
        return -1;
    }
    if (config.orientation != "portrait" && config.orientation != "landscape") {
        std::cerr << "[Error] orientation must be 'portrait' or 'landscape'." << std::endl;
        return -1;
    }
    if (config.startId + config.count > dictSize) {
        std::cerr << "[Error] Requested marker range exceeds dictionary size (" << dictSize << ")." << std::endl;
        return -1;
    }

    const PageGeometry geometry = buildPageGeometry(config.markerSizeMm,
                                                    config.dpi,
                                                    config.layoutMode,
                                                    config.orientation);
    if (geometry.markersPerPage <= 0) {
        std::cerr << "[Error] Marker size is too large for A4 layout." << std::endl;
        return -1;
    }
    if (geometry.columns < 1 || geometry.rows < 1) {
        std::cerr << "[Error] Marker size is too large for A4 page." << std::endl;
        return -1;
    }

    if (geometry.markerSizePx + 2 * geometry.marginPx > geometry.pageWidthPx ||
        geometry.cellHeightPx + 2 * geometry.marginPx > geometry.pageHeightPx) {
        std::cerr << "[Error] Marker size is too large for the selected A4 page setup." << std::endl;
        return -1;
    }

    const std::string outputDir = config.dictName + "_A4_" +
                                  std::to_string(static_cast<int>(std::lround(config.markerSizeMm))) + "mm_" +
                                  config.layoutMode + "_" + config.orientation;
    if (!createDirectoryIfNeeded(outputDir)) {
        return -1;
    }

    cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(dictId);
    const int totalPages = (config.count + geometry.markersPerPage - 1) / geometry.markersPerPage;

    for (int pageIndex = 0; pageIndex < totalPages; ++pageIndex) {
        cv::Mat page(geometry.pageHeightPx, geometry.pageWidthPx, CV_8UC1, cv::Scalar(255));
        const int pageStart = pageIndex * geometry.markersPerPage;
        const int pageEnd = std::min(pageStart + geometry.markersPerPage, config.count);

        for (int localIndex = pageStart; localIndex < pageEnd; ++localIndex) {
            const int markerId = config.startId + localIndex;
            const int slot = localIndex - pageStart;
            int x = 0;
            int y = 0;

            if (isSingleMode(config.layoutMode)) {
                x = (geometry.pageWidthPx - geometry.cellWidthPx) / 2;
                y = (geometry.pageHeightPx - geometry.cellHeightPx) / 2;
            } else {
                const int row = slot / geometry.columns;
                const int col = slot % geometry.columns;
                x = geometry.marginPx + col * (geometry.cellWidthPx + geometry.gapPx);
                y = geometry.marginPx + row * (geometry.cellHeightPx + geometry.gapPx);
            }

            cv::Mat canvas = createMarkerCanvas(dictionary,
                                                markerId,
                                                geometry.markerSizePx,
                                                config.borderBits,
                                                geometry.labelHeightPx,
                                                geometry.cellWidthPx,
                                                geometry.cellHeightPx,
                                                config.dictName,
                                                geometry.actualMarkerSizeMm);
            canvas.copyTo(page(cv::Rect(x, y, canvas.cols, canvas.rows)));
        }

        std::ostringstream title;
        title << config.dictName << "  page " << (pageIndex + 1) << "/" << totalPages
              << "  marker=" << std::fixed << std::setprecision(1) << geometry.actualMarkerSizeMm
              << "mm  A4@" << config.dpi << "dpi  " << config.layoutMode << "  " << config.orientation;
        cv::putText(page, title.str(), cv::Point(geometry.marginPx, geometry.pageHeightPx - geometry.marginPx / 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0), 2);

        const std::string outputPath = outputDir + "/page_" + std::to_string(pageIndex + 1) + ".png";
        if (!cv::imwrite(outputPath, page)) {
            std::cerr << "[Error] Failed to save page: " << outputPath << std::endl;
            return -1;
        }
        std::cout << "[Info] Saved A4 page: " << outputPath << std::endl;
    }

    if (!saveManifest(outputDir, config, geometry, totalPages)) {
        return -1;
    }

    std::cout << "[Info] Finished generating " << config.count << " markers in " << outputDir << std::endl;
    std::cout << "[Info] Actual marker size on A4: " << std::fixed << std::setprecision(2)
              << geometry.actualMarkerSizeMm << " mm" << std::endl;
    std::cout << "[Info] Layout: " << geometry.columns << " columns x " << geometry.rows
              << " rows, " << geometry.markersPerPage << " markers/page" << std::endl;
    return 0;
}
