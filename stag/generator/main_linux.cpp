/**
 * @file main_linux.cpp
 * @brief Linux 平台 STag 标记生成程序实现
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

// --- Cairo & FreeType ---
#include <cairo/cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cairo/cairo-ft.h>

const double PI = 3.14159265358979323846;
const int DEFAULT_HD = 21;
const int DEFAULT_START_ID = 0;
const int DEFAULT_COUNT = 12;
const std::string DEFAULT_OUTPUT_DIR = "";
const std::string DEFAULT_CONFIG_PATH = "./config.yaml";
std::string gExecutableDir = ".";

struct GeneratorConfig {
    int hd;
    int startId;
    int count;
    std::string outputDir;
};

/**
 * @brief 去除字符串首尾空白字符。
 * @param[in] value 输入字符串。
 * @return 去除首尾空白后的字符串。
 */
std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

/**
 * @brief 去除字符串两端成对的单引号或双引号。
 * @param[in] value 输入字符串。
 * @return 去除外层引号后的字符串。
 */
std::string stripQuotes(const std::string& value) {
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

/**
 * @brief 提取路径的父目录。
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
 * @brief 生成 STag 生成器默认配置。
 * @return 默认配置结构体。
 */
GeneratorConfig makeDefaultGeneratorConfig() {
    GeneratorConfig config;
    config.hd = DEFAULT_HD;
    config.startId = DEFAULT_START_ID;
    config.count = DEFAULT_COUNT;
    config.outputDir = DEFAULT_OUTPUT_DIR;
    return config;
}

/**
 * @brief 将字符串解析为整数。
 * @param[in] value 输入字符串。
 * @param[out] out 解析得到的整数。
 * @return 解析成功返回 true，否则返回 false。
 */
bool parseInt(const std::string& value, int& out) {
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

/**
 * @brief 从 YAML 文本配置中读取 STag 生成参数。
 * @param[in] configPath 配置文件路径。
 * @param[out] config 输出配置结构体。
 * @param[out] error 加载失败时返回错误信息。
 * @return 加载成功返回 true，否则返回 false。
 */
bool loadGeneratorConfigFromYaml(const std::string& configPath, GeneratorConfig& config, std::string& error) {
    std::ifstream file(configPath.c_str());
    if (!file.is_open()) {
        error = "Cannot open generator config yaml: " + configPath;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        line = trim(line);
        if (line.empty() || line == "---" || line.find("%YAML") == 0) {
            continue;
        }

        const size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, colonPos));
        const std::string value = stripQuotes(trim(line.substr(colonPos + 1)));
        if (value.empty()) {
            continue;
        }

        int parsed = 0;
        if (key == "hd" || key == "library_hd") {
            if (!parseInt(value, parsed)) {
                error = "Invalid hd value: " + value;
                return false;
            }
            config.hd = parsed;
        } else if (key == "start_id") {
            if (!parseInt(value, parsed)) {
                error = "Invalid start_id value: " + value;
                return false;
            }
            config.startId = parsed;
        } else if (key == "count") {
            if (!parseInt(value, parsed)) {
                error = "Invalid count value: " + value;
                return false;
            }
            config.count = parsed;
        } else if (key == "output_dir") {
            config.outputDir = value;
        }
    }

    return true;
}

/**
 * @brief 判断给定 HD 码库编号是否受支持。
 * @param[in] hd 码库 HD 值。
 * @return 支持返回 true，否则返回 false。
 */
bool isSupportedHd(int hd) {
    return hd == 11 || hd == 13 || hd == 15 || hd == 17 || hd == 19 || hd == 21 || hd == 23;
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
              << "  " << app << " ../../../stag/generator/config.yaml\n";
}

// --- GDI+ 兼容适配层 ---
struct BitmapData {
    int Width;
    int Height;
    int Stride;
    unsigned char* Scan0;
};

struct DoublePoint {
    double x, y;
    /**
     * @brief 构造双精度二维点。
     * @param[in] _x x 坐标。
     * @param[in] _y y 坐标。
     */
    DoublePoint(double _x, double _y) : x(_x), y(_y) {}
};

struct Point {
    int X, Y;
};

/**
 * @brief 若目录不存在则创建目录。
 * @param[in] path 目录路径。
 */
void createDirectory(const std::string& path) {
    struct stat st = {0};
    if (stat(path.c_str(), &st) == -1) {
        mkdir(path.c_str(), 0777);
    }
}

class EDMarkerGenerator {
private:
    int noOfBits = 48;
    float border = 0.125f;
    float outerCircleRadius = 0.4f;
    float innerCircleRadius = 0.35f;
    float codeRadius = 0.062482177287080f;
    float fillerCodeRadius = 0.7f;

    int fileSize = 1000;
    float markerSize, borderSize;
    float outerCircleDiameterSize, innerCircleDiameterSize;
    float outerCircleTopLeft, innerCircleTopLeft;
    float codeCircleDiameterSize, fillerCircleDiameterSize;

    int HD;
    std::vector<std::vector<uint8_t>> codes;
    std::vector<DoublePoint> codeLocs;
    std::vector<std::vector<int>> nearbyCodes;

    // --- 字体相关资源 ---
    FT_Library ftLibrary;
    FT_Face ftFace;
    cairo_font_face_t* cairoFontFace;
    bool fontLoaded;

public:
    /**
     * @brief 构造 STag 生成器并初始化绘制参数与字体资源。
     */
    EDMarkerGenerator() {
        markerSize = fileSize / (1 + border * 2);
        borderSize = markerSize * border;
        outerCircleDiameterSize = 2 * markerSize * outerCircleRadius;
        innerCircleDiameterSize = 2 * markerSize * innerCircleRadius;

        outerCircleTopLeft = (fileSize - outerCircleDiameterSize) / 2;
        innerCircleTopLeft = (fileSize - innerCircleDiameterSize) / 2;
        codeCircleDiameterSize = 2 * innerCircleDiameterSize * codeRadius;
        fillerCircleDiameterSize = codeCircleDiameterSize * fillerCodeRadius;

        // --- 初始化 FreeType 并加载本地字体 ---
        fontLoaded = false;
        cairoFontFace = nullptr;

        if (FT_Init_FreeType(&ftLibrary)) {
            std::cerr << "Error: Could not init FreeType library" << std::endl;
        } else {
            std::string fontPath = joinPath(gExecutableDir, "../front/TrueType/Century_Gothic_Regular.TTF");
            if (FT_New_Face(ftLibrary, fontPath.c_str(), 0, &ftFace)) {
                std::cerr << "Error: Could not open font file: " << fontPath << std::endl;
                std::cerr << "Falling back to system font." << std::endl;
            } else {
                // 创建 Cairo 字体对象
                cairoFontFace = cairo_ft_font_face_create_for_ft_face(ftFace, 0);
                fontLoaded = true;
                std::cout << "Successfully loaded font: " << fontPath << std::endl;
            }
        }

        fillLocs();
    }

    /**
     * @brief 析构生成器并释放 FreeType 与 Cairo 字体资源。
     */
    ~EDMarkerGenerator() {
        if (cairoFontFace) {
            cairo_font_face_destroy(cairoFontFace);
        }
        if (fontLoaded) {
            FT_Done_Face(ftFace);
        }
        FT_Done_FreeType(ftLibrary);
    }

    /**
     * @brief 按配置批量生成指定 HD 和 ID 范围的 STag 标记。
     * @param[in] config 生成配置。
     * @return 成功返回 0，失败返回 -1。
     */
    int run(const GeneratorConfig& config) {
        if (!isSupportedHd(config.hd)) {
            std::cerr << "[Error] hd must be one of 11, 13, 15, 17, 19, 21, 23." << std::endl;
            return -1;
        }
        if (config.startId < 0 || config.count <= 0) {
            std::cerr << "[Error] start_id must be >= 0 and count must be > 0." << std::endl;
            return -1;
        }

        HD = config.hd;
        if (!readCodeList()) {
            std::cerr << "[Error] Could not read file ../codes/HD" << HD << ".txt" << std::endl;
            return -1;
        }
        if (config.startId + config.count > static_cast<int>(codes.size())) {
            std::cerr << "[Error] Requested marker range exceeds HD" << HD
                      << " code count (" << codes.size() << ")." << std::endl;
            return -1;
        }

        const std::string outputDir = config.outputDir.empty()
                                      ? ("HD" + std::to_string(HD) + "_ID" + std::to_string(config.startId) + "_" +
                                         std::to_string(config.startId + config.count - 1))
                                      : config.outputDir;
        drawMarkers(config.startId, config.count, outputDir);
        std::cout << "[Info] Finished generating " << config.count << " STag markers from HD" << HD
                  << ", ID " << config.startId << " - " << (config.startId + config.count - 1)
                  << " in " << outputDir << std::endl;
        return 0;
    }

private:
    // --- GDI+ 模拟函数 ---
    /**
     * @brief 锁定 Cairo 图像表面并暴露像素缓冲区信息。
     * @param[in] surface Cairo 图像表面。
     * @param[out] bd 输出的位图访问信息。
     */
    void LockBits(cairo_surface_t* surface, BitmapData* bd) {
        cairo_surface_flush(surface);
        bd->Width = cairo_image_surface_get_width(surface);
        bd->Height = cairo_image_surface_get_height(surface);
        bd->Stride = cairo_image_surface_get_stride(surface);
        bd->Scan0 = cairo_image_surface_get_data(surface);
    }

    /**
     * @brief 解锁 Cairo 图像表面并标记内容已修改。
     * @param[in] surface Cairo 图像表面。
     */
    void UnlockBits(cairo_surface_t* surface) {
        cairo_surface_mark_dirty(surface);
    }

    /**
     * @brief 判断指定像素是否为白色。
     * @param[in] bd 位图数据。
     * @param[in] x x 坐标。
     * @param[in] y y 坐标。
     * @return 白色返回 true，否则返回 false。
     */
    inline bool isWhite(BitmapData* bd, int x, int y) {
        unsigned char* b = bd->Scan0 + y * bd->Stride + x * 4;
        return (b[0] == 255 && b[1] == 255 && b[2] == 255);
    }
    /**
     * @brief 判断指定像素是否为黑色。
     * @param[in] bd 位图数据。
     * @param[in] x x 坐标。
     * @param[in] y y 坐标。
     * @return 黑色返回 true，否则返回 false。
     */
    inline bool isBlack(BitmapData* bd, int x, int y) {
        unsigned char* b = bd->Scan0 + y * bd->Stride + x * 4;
        return (b[0] == 0 && b[1] == 0 && b[2] == 0);
    }
    /**
     * @brief 读取指定像素的灰度通道值。
     * @param[in] bd 位图数据。
     * @param[in] x x 坐标。
     * @param[in] y y 坐标。
     * @return 像素灰度值。
     */
    inline int readPix(BitmapData* bd, int x, int y) {
        return (bd->Scan0 + y * bd->Stride + x * 4)[0];
    }

    /**
     * @brief 将极坐标转换为归一化笛卡尔坐标。
     * @param[in] radius 归一化半径。
     * @param[in] radians 角度，单位弧度。
     * @return 转换后的二维点。
     */
    DoublePoint polarToCart(double radius, double radians) {
        return DoublePoint(0.5 + std::cos(radians) * radius,
                           0.5 - std::sin(radians) * radius);
    }

    /**
     * @brief 计算两个双精度点之间的欧氏距离。
     * @param[in] p1 点 1。
     * @param[in] p2 点 2。
     * @return 两点距离。
     */
    double distanceBetweenDoublePoints(DoublePoint p1, DoublePoint p2) {
        return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
    }

    /**
     * @brief 生成 48 个编码点位置及其邻接关系。
     */
    void fillLocs() {
        codeLocs.clear();
        for (int i = 0; i < 4; i++) {
             codeLocs.push_back(polarToCart(0.088363142525988, 0.785398163397448 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.206935928182607, 0.459275804122858 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.206935928182607, (PI / 2) - 0.459275804122858 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.313672146827381, 0.200579720495241 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.327493143484516, 0.591687617505840 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.327493143484516, (PI / 2) - 0.591687617505840 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.313672146827381, (PI / 2) - 0.200579720495241 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.437421957035861, 0.145724938287167 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.437226762361658, 0.433363129825345 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.430628029742607, 0.785398163397448 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.437226762361658, (PI / 2) - 0.433363129825345 + i * (PI / 2)));
             codeLocs.push_back(polarToCart(0.437421957035861, (PI / 2) - 0.145724938287167 + i * (PI / 2)));
        }

        nearbyCodes.clear();
        for (int i = 0; i < noOfBits; i++) {
            std::vector<int> nearby;
            for (int j = 0; j < noOfBits; j++) {
                if (i == j) continue;
                if (distanceBetweenDoublePoints(codeLocs[i], codeLocs[j]) < codeRadius * 4)
                    nearby.push_back(j);
            }
            nearbyCodes.push_back(nearby);
        }
    }

    /**
     * @brief 生成圆形结构元素对应的邻域掩码。
     * @param[in] r 掩码半径。
     * @return 邻域点偏移列表。
     */
    std::vector<Point> generateBallMask(int r) {
        std::vector<Point> maskList;
        for (int i = -r; i <= r; i++)
            for (int j = -r; j <= r; j++)
                if (!(i == 0 && j == 0) && (i * i + j * j <= r * r))
                    maskList.push_back({i, j});
        return maskList;
    }

    /**
     * @brief 统计邻域中满足黑/白条件的像素数。
     * @param[in] bd 位图数据。
     * @param[in] x 中心像素 x 坐标。
     * @param[in] y 中心像素 y 坐标。
     * @param[in] nonBlack 为 true 时统计非黑像素，否则统计非白像素。
     * @param[in] list 邻域偏移列表。
     * @return 满足条件的像素个数。
     */
    int countNonPixels(BitmapData* bd, int x, int y, bool nonBlack, const std::vector<Point>& list) {
        int tot = 0;
        for (const auto& pt : list) {
            int nx = x + pt.X, ny = y + pt.Y;
            if (nx >= 0 && nx < bd->Width && ny >= 0 && ny < bd->Height) {
                if (nonBlack) {
                    if (!isBlack(bd, nx, ny)) tot++;
                } else {
                    if (!isWhite(bd, nx, ny)) tot++;
                }
            }
        }
        return tot;
    }

    /**
     * @brief 对位图执行膨胀或腐蚀后处理。
     * @param[in] surface Cairo 图像表面。
     * @param[in] dilate 为 true 时执行膨胀，否则执行腐蚀。
     * @param[in] radius 结构元素半径。
     */
    void processBitmap(cairo_surface_t* surface, bool dilate, int radius) {
        BitmapData imgData;
        LockBits(surface, &imgData);

        std::vector<unsigned char> refBuffer(imgData.Stride * imgData.Height);
        std::memcpy(refBuffer.data(), imgData.Scan0, imgData.Stride * imgData.Height);

        BitmapData refData = imgData;
        refData.Scan0 = refBuffer.data();

        std::vector<Point> list = generateBallMask(radius);
        int borderLimit = (int)outerCircleTopLeft;
        int threshold = (int)list.size() / 2;

        for (int j = borderLimit; j < fileSize - borderLimit; j++) {
            for (int k = borderLimit; k < fileSize - borderLimit; k++) {
                unsigned char* p = imgData.Scan0 + k * imgData.Stride + j * 4;
                if (dilate) {
                    if (!isBlack(&refData, j, k)) {
                        if (countNonPixels(&refData, j, k, false, list) > threshold) {
                            p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 255;
                        }
                    }
                } else {
                    if (!isWhite(&refData, j, k)) {
                        if (countNonPixels(&refData, j, k, true, list) > threshold) {
                            p[0] = 255; p[1] = 255; p[2] = 255; p[3] = 255;
                        }
                    }
                }
            }
        }
        UnlockBits(surface);
    }

    /**
     * @brief 对位图执行平滑处理，改善边缘锯齿。
     * @param[in] surface Cairo 图像表面。
     */
    void smoothBitmap(cairo_surface_t* surface) {
        BitmapData imgData;
        LockBits(surface, &imgData);

        std::vector<unsigned char> refBuffer(imgData.Stride * imgData.Height);
        std::memcpy(refBuffer.data(), imgData.Scan0, imgData.Stride * imgData.Height);

        BitmapData refData = imgData;
        refData.Scan0 = refBuffer.data();

        int w = imgData.Width;
        int h = imgData.Height;

        for (int i = 0; i < w; i++) {
            for (int j = 0; j < h; j++) {
                double dist = std::sqrt(std::pow(i - w / 2.0, 2) + std::pow(j - h / 2.0, 2));
                if (dist < w / 2.0 - outerCircleTopLeft - 10) {
                    if (isBlack(&refData, i, j)) {
                        bool hasWhite = false;
                        int total = 0;
                        for (int n1 = -1; n1 <= 1; n1++) {
                            for (int n2 = -1; n2 <= 1; n2++) {
                                if (i + n1 >= 0 && i + n1 < w && j + n2 >= 0 && j + n2 < h) {
                                    if (isWhite(&refData, i + n1, j + n2) && std::abs(n1 + n2) < 2)
                                        hasWhite = true;
                                    total += readPix(&refData, i + n1, j + n2);
                                }
                            }
                        }
                        if (hasWhite) {
                            unsigned char val = (unsigned char)(total / 9.0);
                            unsigned char* p = imgData.Scan0 + j * imgData.Stride + i * 4;
                            p[0] = val; p[1] = val; p[2] = val; p[3] = 255;
                        }
                    }
                }
            }
        }
        UnlockBits(surface);
    }

    /**
     * @brief 从码表文件读取当前 HD 对应的二进制编码。
     * @return 读取成功返回 true，否则返回 false。
     */
    bool readCodeList() {
        codes.clear();
        std::string filename = joinPath(gExecutableDir, "../codes/HD" + std::to_string(HD) + ".txt");
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line.back() == '\r') line.pop_back();
            std::vector<uint8_t> codeLine;
            for (size_t j = 0; j < (size_t)noOfBits && j < line.length(); j++)
                codeLine.push_back((uint8_t)(line[j] - '0'));
            if (codeLine.size() == (size_t)noOfBits)
                codes.push_back(codeLine);
        }
        return true;
    }

    // 辅助函数：绘制带间距的文字
    // cr: Cairo上下文
    // text: 字符串
    // spacing: 字间距像素值
    // offsetX, offsetY: 整体位置微调
    /**
     * @brief 使用 Cairo 按固定字间距绘制文本。
     * @param[in] cr Cairo 绘图上下文。
     * @param[in] text 待绘制文本。
     * @param[in] spacing 字间距，单位像素。
     * @param[in] offsetX 整体 x 偏移。
     * @param[in] offsetY 整体 y 偏移。
     */
    void drawSpacedText(cairo_t* cr, const std::string& text, double spacing, double offsetX, double offsetY) {
        if (text.empty()) return;

        // 1. 计算总宽度 (所有字符宽度 + 间距)
        double totalWidth = 0;
        std::vector<double> charAdvances;
        cairo_text_extents_t ext;

        for (char c : text) {
            std::string s(1, c);
            cairo_text_extents(cr, s.c_str(), &ext);
            charAdvances.push_back(ext.x_advance);
            totalWidth += ext.x_advance;
        }
        if (text.length() > 1) {
            totalWidth += (text.length() - 1) * spacing;
        }

        // 2. 计算垂直居中基准 (使用整个字符串的高度，保证垂直位置稳定)
        cairo_text_extents_t wholeExt;
        cairo_text_extents(cr, text.c_str(), &wholeExt);

        // 起始 X 坐标 (居中 + 偏移)
        double cursorX = -totalWidth / 2.0 + offsetX;
        // 起始 Y 坐标 (基线位置：抵消字形高度和 bearing，加上偏移)
        double cursorY = -wholeExt.height / 2.0 - wholeExt.y_bearing + offsetY;

        // 3. 逐字绘制
        for (size_t i = 0; i < text.length(); ++i) {
            std::string s(1, text[i]);
            cairo_move_to(cr, cursorX, cursorY);
            cairo_show_text(cr, s.c_str());

            // 移动光标：字符本身宽度 + 额外间距
            cursorX += charAdvances[i] + spacing;
        }
    }

    /**
     * @brief 按指定起始 ID 和数量绘制并保存 STag 图片。
     * @param[in] startId 起始标记 ID。
     * @param[in] count 生成数量。
     * @param[in] dirName 输出目录。
     */
    void drawMarkers(int startId, int count, const std::string& dirName) {
        createDirectory(dirName);

        // ==========================================
        //  参 数 调 节 区
        // ==========================================

        // 1. 几何对齐修正 (+0.5 对齐像素网格)
        const double GDI_OFFSET = 0.5;

        // 2. 编号文字 (中间大数字) 参数
        // NUM_SPACING: 字间距 (如果数字太挤，增大此值；太散，减小此值)
        const double NUM_SPACING  = 1;
        const double NUM_OFFSET_X = -2; // 左右微调
        const double NUM_OFFSET_Y = -8; // 上下微调 (向下为正)

        // 3. HD 编号 (右下角小字) 参数
        const double HD_SPACING   = 1;  // 字间距
        const double HD_OFFSET_X  = -2;
        const double HD_OFFSET_Y  = -2;

        // ==========================================

        for (int markerId = startId; markerId < startId + count; markerId++) {
            cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, fileSize, fileSize);
            cairo_t* cr = cairo_create(surface);

            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_paint(cr);

            // === 绘制几何图形 ===
            cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
            cairo_set_source_rgb(cr, 0, 0, 0);

            cairo_rectangle(cr, borderSize + GDI_OFFSET, borderSize + GDI_OFFSET, markerSize, markerSize);
            cairo_fill(cr);

            cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_arc(cr, outerCircleTopLeft + outerCircleDiameterSize/2 + GDI_OFFSET,
                          outerCircleTopLeft + outerCircleDiameterSize/2 + GDI_OFFSET,
                          outerCircleDiameterSize/2, 0, 2*PI);
            cairo_fill(cr);

            cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
            cairo_set_source_rgb(cr, 0, 0, 0);
            for (int j = 0; j < noOfBits; j++) {
                if (codes[markerId][j] == 1) {
                    double cx = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].x + GDI_OFFSET;
                    double cy = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].y + GDI_OFFSET;
                    cairo_new_path(cr);
                    cairo_arc(cr, cx, cy, codeCircleDiameterSize/2, 0, 2*PI);
                    cairo_fill(cr);
                }
            }

            for (int j = 0; j < noOfBits; j++) {
                for (int k = j + 1; k < noOfBits; k++) {
                    if ((codes[markerId][j] == 1) && (codes[markerId][k] == 1)) {
                         if (std::find(nearbyCodes[j].begin(), nearbyCodes[j].end(), k) != nearbyCodes[j].end()) {
                            double cx = innerCircleTopLeft + innerCircleDiameterSize * ((codeLocs[j].x + codeLocs[k].x) / 2) + GDI_OFFSET;
                            double cy = innerCircleTopLeft + innerCircleDiameterSize * ((codeLocs[j].y + codeLocs[k].y) / 2) + GDI_OFFSET;
                            cairo_new_path(cr);
                            cairo_arc(cr, cx, cy, fillerCircleDiameterSize/2, 0, 2*PI);
                            cairo_fill(cr);
                         }
                    }
                }
            }

            cairo_set_source_rgb(cr, 1, 1, 1);
            for (int j = 0; j < noOfBits; j++) {
                if (codes[markerId][j] == 0) {
                    double cx = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].x + GDI_OFFSET;
                    double cy = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].y + GDI_OFFSET;
                    cairo_new_path(cr);
                    cairo_arc(cr, cx, cy, codeCircleDiameterSize/2, 0, 2*PI);
                    cairo_fill(cr);
                }
            }

            cairo_destroy(cr);

            // === 图像后处理 ===
            int rad = 12;
            for (int j = 0; j < 5; j++) {
                processBitmap(surface, true, rad);
                processBitmap(surface, false, rad);
            }
            smoothBitmap(surface);

            // === 重新绘制清晰点 ===
            cr = cairo_create(surface);
            cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

            cairo_set_source_rgb(cr, 0, 0, 0);
            for (int j = 0; j < noOfBits; j++) {
                if (codes[markerId][j] == 1) {
                    double cx = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].x + GDI_OFFSET;
                    double cy = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].y + GDI_OFFSET;
                    cairo_new_path(cr);
                    cairo_arc(cr, cx, cy, codeCircleDiameterSize/2, 0, 2*PI);
                    cairo_fill(cr);
                }
            }
            cairo_set_source_rgb(cr, 1, 1, 1);
            for (int j = 0; j < noOfBits; j++) {
                if (codes[markerId][j] == 0) {
                    double cx = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].x + GDI_OFFSET;
                    double cy = innerCircleTopLeft + innerCircleDiameterSize * codeLocs[j].y + GDI_OFFSET;
                    cairo_new_path(cr);
                    cairo_arc(cr, cx, cy, codeCircleDiameterSize/2, 0, 2*PI);
                    cairo_fill(cr);
                }
            }

            // === Ring Mask ===
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
            cairo_new_path(cr);
            cairo_arc(cr, outerCircleTopLeft + outerCircleDiameterSize/2 + GDI_OFFSET,
                          outerCircleTopLeft + outerCircleDiameterSize/2 + GDI_OFFSET,
                          outerCircleDiameterSize/2, 0, 2*PI);
            cairo_arc(cr, innerCircleTopLeft + innerCircleDiameterSize/2 + GDI_OFFSET,
                          innerCircleTopLeft + innerCircleDiameterSize/2 + GDI_OFFSET,
                          innerCircleDiameterSize/2 + 1, 0, 2*PI);
            cairo_fill(cr);
            cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

            // === 绘制文字 (支持字间距调节) ===
            if (fontLoaded) {
                cairo_set_font_face(cr, cairoFontFace);
            } else {
                cairo_select_font_face(cr, "Century Gothic", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            }
            cairo_set_source_rgb(cr, 1, 1, 1);

            // --- 绘制 编号 ---
            cairo_save(cr);
            float centerString = (float)(2.375 * borderSize);
            cairo_translate(cr, centerString + GDI_OFFSET, centerString + GDI_OFFSET);
            cairo_rotate(cr, 315 * (PI / 180.0));
            cairo_set_font_size(cr, 72.0 * 1.333);

            std::string sIdx = std::to_string(markerId);
            size_t len = std::to_string(startId + count - 1).length();
            while (sIdx.length() != len) sIdx = "0" + sIdx;

            // 调用自定义绘制函数
            drawSpacedText(cr, sIdx, NUM_SPACING, NUM_OFFSET_X, NUM_OFFSET_Y);

            cairo_restore(cr);

            // --- 绘制 HD 编号 ---
            cairo_save(cr);
            centerString = (float)(1.875 * borderSize);
            cairo_translate(cr, centerString + GDI_OFFSET, centerString + GDI_OFFSET);
            cairo_rotate(cr, 315 * (PI / 180.0));
            cairo_set_font_size(cr, 20.0 * 1.333);
            std::string hdStr = "HD" + std::to_string(HD);

            // 调用自定义绘制函数
            drawSpacedText(cr, hdStr, HD_SPACING, HD_OFFSET_X, HD_OFFSET_Y);

            cairo_restore(cr);

            // 保存
            std::string idStrA = std::to_string(markerId);
            while (idStrA.length() < 5) idStrA = "0" + idStrA;
            std::string pathStr = dirName + "/" + idStrA + ".png";

            cairo_surface_write_to_png(surface, pathStr.c_str());
            std::cout << "Saved: " << pathStr << std::endl;

            cairo_destroy(cr);
            cairo_surface_destroy(surface);
        }
    }
};

/**
 * @brief 程序入口，执行 Linux 平台 STag 标记生成流程。
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

    gExecutableDir = dirnameOf(argv[0]);
    const std::string configPath = (argc == 2) ? argv[1] : joinPath(gExecutableDir, DEFAULT_CONFIG_PATH);
    GeneratorConfig config = makeDefaultGeneratorConfig();
    std::string error;
    if (!loadGeneratorConfigFromYaml(configPath, config, error)) {
        std::cerr << "[Error] " << error << std::endl;
        return -1;
    }

    EDMarkerGenerator generator;
    return generator.run(config);
}
