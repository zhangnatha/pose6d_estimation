/**
 * @file main_windows.cpp
 * @brief Windows 平台 STag 标记生成程序实现
 * @author zhangnatha
 * @date 2025.12.21
 * @version 1.0
 * @email zhangnatha@qq.com
 */

#include <algorithm>
#include <cmath>
#include <cstdint> 

#include <fstream>
#include <iostream>
#include <string>
#include <vector>


#ifndef NOMINMAX
#define NOMINMAX 
#endif
#include <windows.h> 



#if defined(__MINGW32__) || defined(__MINGW64__)
#ifndef PROPID
typedef ULONG PROPID;
#endif
#endif

#include <gdiplus.h>


#pragma comment(lib, "Gdiplus.lib")

using namespace Gdiplus;

const double PI = 3.14159265358979323846;


struct DoublePoint {
  double x, y;
  /**
   * @brief 构造双精度二维点。
   * @param[in] _x x 坐标。
   * @param[in] _y y 坐标。
   */
  DoublePoint(double _x, double _y) : x(_x), y(_y) {}
};



/**
 * @brief 将窄字符串转换为宽字符串。
 * @param[in] str 输入字符串。
 * @return 转换后的宽字符串。
 */
std::wstring StringToWString(const std::string &str) {
  if (str.empty())
    return std::wstring();
  return std::wstring(str.begin(), str.end());
}


/**
 * @brief 获取指定图像格式对应的编码器 CLSID。
 * @param[in] format 图像 MIME 类型。
 * @param[out] pClsid 输出编码器 CLSID。
 * @return 找到时返回编码器索引，失败返回 -1。
 */
int GetEncoderClsid(const WCHAR *format, CLSID *pClsid) {
  UINT num = 0;
  UINT size = 0;
  GetImageEncodersSize(&num, &size);
  if (size == 0)
    return -1;
  ImageCodecInfo *pImageCodecInfo = (ImageCodecInfo *)(malloc(size));
  if (pImageCodecInfo == NULL)
    return -1;
  GetImageEncoders(num, size, pImageCodecInfo);
  for (UINT j = 0; j < num; ++j) {
    if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
      *pClsid = pImageCodecInfo[j].Clsid;
      free(pImageCodecInfo);
      return j;
    }
  }
  free(pImageCodecInfo);
  return -1;
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
  std::vector<std::vector<int>>
      nearbyCodes; 
                   

  
  Font *largeFont;
  Font *smallFont;

public:
  /**
   * @brief 构造 STag 生成器并初始化几何参数、字体和点位。
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

    
    FontFamily fontFamily(L"Century Gothic");
    largeFont = new Font(&fontFamily, 72, FontStyleRegular, UnitPoint);
    smallFont = new Font(&fontFamily, 20, FontStyleRegular, UnitPoint);

    
    fillLocs();
  }

  /**
   * @brief 析构生成器并释放字体对象。
   */
  ~EDMarkerGenerator() {
    delete largeFont;
    delete smallFont;
  }

  
  /**
   * @brief 依次生成所有受支持 HD 类型的 STag 标记。
   */
  void run() {
    
    for (HD = 11; HD <= 23; HD += 2) {
      if (readCodeList()) {
        drawMarkers();
      } else {
        std::cerr << "Error: Could not read file HD" << HD << ".txt"
                  << std::endl;
      }
    }
  }

private:
  
  
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
   * @brief 计算两个点之间的欧氏距离。
   * @param[in] p1 点 1。
   * @param[in] p2 点 2。
   * @return 两点间距离。
   */
  double distanceBetweenDoublePoints(DoublePoint p1, DoublePoint p2) {
    return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
  }

  
  /**
   * @brief 初始化编码点位置和相邻点索引关系。
   */
  void fillLocs() {
    codeLocs.clear();
    
    for (int i = 0; i < 4; i++) {
      codeLocs.push_back(
          polarToCart(0.088363142525988, 0.785398163397448 + i * (PI / 2)));
      
      codeLocs.push_back(
          polarToCart(0.206935928182607, 0.459275804122858 + i * (PI / 2)));
      codeLocs.push_back(polarToCart(
          0.206935928182607, (PI / 2) - 0.459275804122858 + i * (PI / 2)));
      codeLocs.push_back(
          polarToCart(0.313672146827381, 0.200579720495241 + i * (PI / 2)));
      codeLocs.push_back(
          polarToCart(0.327493143484516, 0.591687617505840 + i * (PI / 2)));
      codeLocs.push_back(polarToCart(
          0.327493143484516, (PI / 2) - 0.591687617505840 + i * (PI / 2)));
      codeLocs.push_back(polarToCart(
          0.313672146827381, (PI / 2) - 0.200579720495241 + i * (PI / 2)));
      codeLocs.push_back(
          polarToCart(0.437421957035861, 0.145724938287167 + i * (PI / 2)));
      codeLocs.push_back(
          polarToCart(0.437226762361658, 0.433363129825345 + i * (PI / 2)));
      codeLocs.push_back(
          polarToCart(0.430628029742607, 0.785398163397448 + i * (PI / 2)));
      codeLocs.push_back(polarToCart(
          0.437226762361658, (PI / 2) - 0.433363129825345 + i * (PI / 2)));
      codeLocs.push_back(polarToCart(
          0.437421957035861, (PI / 2) - 0.145724938287167 + i * (PI / 2)));
    }

    
    
    nearbyCodes.clear();
    for (int i = 0; i < noOfBits; i++) {
      std::vector<int> nearby;
      for (int j = 0; j < noOfBits; j++) {
        if (i == j)
          continue;
        if (distanceBetweenDoublePoints(codeLocs[i], codeLocs[j]) <
            codeRadius * 4)
          nearby.push_back(j);
      }
      nearbyCodes.push_back(nearby);
    }
  }

  struct Point {
    int X, Y;
  };

  
  /**
   * @brief 生成圆形结构元素掩码。
   * @param[in] r 掩码半径。
   * @return 邻域偏移列表。
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
   * @brief 判断指定像素是否为白色。
   * @param[in] bd 位图数据。
   * @param[in] x x 坐标。
   * @param[in] y y 坐标。
   * @return 白色返回 true，否则返回 false。
   */
  inline bool isWhite(BitmapData *bd, int x, int y) {
    byte *b = (byte *)bd->Scan0 + y * bd->Stride + x * 4;
    return (b[0] == 255 && b[1] == 255 && b[2] == 255);
  }
  /**
   * @brief 判断指定像素是否为黑色。
   * @param[in] bd 位图数据。
   * @param[in] x x 坐标。
   * @param[in] y y 坐标。
   * @return 黑色返回 true，否则返回 false。
   */
  inline bool isBlack(BitmapData *bd, int x, int y) {
    byte *b = (byte *)bd->Scan0 + y * bd->Stride + x * 4;
    return (b[0] == 0 && b[1] == 0 && b[2] == 0);
  }
  /**
   * @brief 读取指定像素灰度通道值。
   * @param[in] bd 位图数据。
   * @param[in] x x 坐标。
   * @param[in] y y 坐标。
   * @return 像素值。
   */
  inline int readPix(BitmapData *bd, int x, int y) {
    return ((byte *)bd->Scan0 + y * bd->Stride + x * 4)[0];
  }

  
  /**
   * @brief 统计邻域内满足黑/白条件的像素个数。
   * @param[in] bd 位图数据。
   * @param[in] x 中心像素 x 坐标。
   * @param[in] y 中心像素 y 坐标。
   * @param[in] nonBlack 为 true 时统计非黑像素，否则统计非白像素。
   * @param[in] list 邻域偏移列表。
   * @return 满足条件的像素数量。
   */
  int countNonPixels(BitmapData *bd, int x, int y, bool nonBlack,
                     const std::vector<Point> &list) {
    int tot = 0;
    for (const auto &pt : list) {
      int nx = x + pt.X, ny = y + pt.Y;
      if (nx >= 0 && nx < (int)bd->Width && ny >= 0 && ny < (int)bd->Height) {
        if (nonBlack) {
          if (!isBlack(bd, nx, ny))
            tot++;
        } 
        else {
          if (!isWhite(bd, nx, ny))
            tot++;
        } 
      }
    }
    return tot;
  }

  
  
  /**
   * @brief 对位图执行膨胀或腐蚀处理。
   * @param[in] b GDI+ 位图对象。
   * @param[in] dilate 为 true 时执行膨胀，否则执行腐蚀。
   * @param[in] radius 结构元素半径。
   */
  void processBitmap(Bitmap *b, bool dilate, int radius) {
    Rect rect(0, 0, b->GetWidth(), b->GetHeight());
    BitmapData imgData;
    b->LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB,
                &imgData); 

    
    Bitmap *refImage = b->Clone(rect, PixelFormat32bppARGB);
    BitmapData refData;
    refImage->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB,
                       &refData); 

    std::vector<Point> list = generateBallMask(radius);
    int borderLimit = (int)outerCircleTopLeft;
    int threshold =
        (int)list.size() /
        2; 

    for (int j = borderLimit; j < fileSize - borderLimit; j++) {
      for (int k = borderLimit; k < fileSize - borderLimit; k++) {
        if (dilate) {
          
          
          if (!isBlack(&refData, j, k)) {
            if (countNonPixels(&refData, j, k, false, list) > threshold) {
              byte *p = (byte *)imgData.Scan0 + k * imgData.Stride + j * 4;
              p[0] = 0;
              p[1] = 0;
              p[2] = 0;
              p[3] = 255;
            }
          }
        } else { 
          
          
          if (!isWhite(&refData, j, k)) {
            if (countNonPixels(&refData, j, k, true, list) > threshold) {
              byte *p = (byte *)imgData.Scan0 + k * imgData.Stride + j * 4;
              p[0] = 255;
              p[1] = 255;
              p[2] = 255;
              p[3] = 255;
            }
          }
        }
      }
    }
    
    refImage->UnlockBits(&refData);
    delete refImage;
    b->UnlockBits(&imgData);
  }

  
  /**
   * @brief 对位图执行 3x3 平滑滤波。
   * @param[in] b GDI+ 位图对象。
   */
  void smoothBitmap(Bitmap *b) {
    Rect rect(0, 0, b->GetWidth(), b->GetHeight());
    BitmapData imgData;
    b->LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &imgData);
    Bitmap *refImage = b->Clone(rect, PixelFormat32bppARGB);
    BitmapData refData;
    refImage->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB,
                       &refData);

    for (int i = 0; i < (int)b->GetWidth(); i++) {
      for (int j = 0; j < (int)b->GetHeight(); j++) {
        
        double dist = std::sqrt(std::pow(i - b->GetWidth() / 2.0, 2) +
                                std::pow(j - b->GetHeight() / 2.0, 2));
        if (dist < b->GetWidth() / 2.0 - outerCircleTopLeft - 10) {
          if (isBlack(&refData, i, j)) {
            bool hasWhite = false;
            int total = 0;
            
            for (int n1 = -1; n1 <= 1; n1++) {
              for (int n2 = -1; n2 <= 1; n2++) {
                if (i + n1 >= 0 && i + n1 < (int)b->GetWidth() && j + n2 >= 0 &&
                    j + n2 < (int)b->GetHeight()) {
                  if (isWhite(&refData, i + n1, j + n2) &&
                      std::abs(n1 + n2) < 2)
                    hasWhite = true;
                  total += readPix(&refData, i + n1, j + n2);
                }
              }
            }
            
            if (hasWhite) {
              byte val = (byte)(total / 9.0);
              byte *p = (byte *)imgData.Scan0 + j * imgData.Stride + i * 4;
              p[0] = val;
              p[1] = val;
              p[2] = val;
              p[3] = 255;
            }
          }
        }
      }
    }
    refImage->UnlockBits(&refData);
    delete refImage;
    b->UnlockBits(&imgData);
  }

  
  /**
   * @brief 从文本文件读取当前 HD 码表。
   * @return 读取成功返回 true，否则返回 false。
   */
  bool readCodeList() {
    codes.clear();
    std::string filename = "../codes/HD" + std::to_string(HD) + ".txt";
    std::ifstream file(filename);
    if (!file.is_open())
      return false;
    std::string line;
    while (std::getline(file, line)) {
      if (line.empty())
        continue;
      if (line.back() == '\r')
        line.pop_back(); 
      std::vector<uint8_t> codeLine;
      for (size_t j = 0; j < (size_t)noOfBits && j < line.length(); j++)
        codeLine.push_back((uint8_t)(line[j] - '0'));
      if (codeLine.size() == (size_t)noOfBits)
        codes.push_back(codeLine);
    }
    return true;
  }

  /**
   * @brief 将序号转换为固定位数的宽字符串。
   * @param[in] index 编号。
   * @return 零填充后的宽字符串。
   */
  std::wstring createIndexString(int index) {
    std::string s = std::to_string(index);
    size_t len = std::to_string(codes.size()).length();
    while (s.length() != len)
      s = "0" + s; 
    return StringToWString(s);
  }

  
  /**
   * @brief 绘制当前 HD 码表下的全部标记并保存为 PNG。
   */
  void drawMarkers() {
    
    std::string dirName = "HD" + std::to_string(HD);
    if (GetFileAttributesA(dirName.c_str()) == INVALID_FILE_ATTRIBUTES) {
      CreateDirectoryA(dirName.c_str(), NULL);
    }

    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    SolidBrush blackBrush(Color::Black);
    SolidBrush whiteBrush(Color::White);

    for (size_t i = 0; i < codes.size(); i++) {
      Bitmap *img = new Bitmap(fileSize, fileSize, PixelFormat32bppARGB);
      Graphics *g = Graphics::FromImage(img);
      g->Clear(Color::White);

      
      
      g->SetSmoothingMode(SmoothingModeNone);
      g->FillRectangle(&blackBrush, borderSize, borderSize, markerSize,
                       markerSize); 

      g->SetSmoothingMode(SmoothingModeAntiAlias);
      g->FillEllipse(&whiteBrush, outerCircleTopLeft, outerCircleTopLeft,
                     outerCircleDiameterSize,
                     outerCircleDiameterSize); 

      g->SetSmoothingMode(SmoothingModeNone);
      
      for (int j = 0; j < noOfBits; j++)
        if (codes[i][j] == 1)
          g->FillEllipse(&blackBrush,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].x -
                             codeCircleDiameterSize / 2,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].y -
                             codeCircleDiameterSize / 2,
                         codeCircleDiameterSize, codeCircleDiameterSize);

      
      for (int j = 0; j < noOfBits; j++)
        for (int k = j + 1; k < noOfBits; k++)
          if ((codes[i][j] == 1) && (codes[i][k] == 1))
            if (std::find(nearbyCodes[j].begin(), nearbyCodes[j].end(), k) !=
                nearbyCodes[j].end())
              g->FillEllipse(
                  &blackBrush,
                  innerCircleTopLeft +
                      innerCircleDiameterSize *
                          (float)((codeLocs[j].x + codeLocs[k].x) / 2) -
                      fillerCircleDiameterSize / 2,
                  innerCircleTopLeft +
                      innerCircleDiameterSize *
                          (float)((codeLocs[j].y + codeLocs[k].y) / 2) -
                      fillerCircleDiameterSize / 2,
                  fillerCircleDiameterSize, fillerCircleDiameterSize);

      
      for (int j = 0; j < noOfBits; j++)
        if (codes[i][j] == 0)
          g->FillEllipse(&whiteBrush,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].x -
                             codeCircleDiameterSize / 2,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].y -
                             codeCircleDiameterSize / 2,
                         codeCircleDiameterSize, codeCircleDiameterSize);

      delete g; 

      
      int rad = 12;
      for (int j = 0; j < 5; j++) {
        processBitmap(img, true, rad);
        processBitmap(img, false, rad);
      }
      smoothBitmap(img);

      
      g = Graphics::FromImage(img);
      g->SetSmoothingMode(SmoothingModeHighQuality);
      for (int j = 0; j < noOfBits; j++)
        if (codes[i][j] == 1)
          g->FillEllipse(&blackBrush,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].x -
                             codeCircleDiameterSize / 2,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].y -
                             codeCircleDiameterSize / 2,
                         codeCircleDiameterSize, codeCircleDiameterSize);
      for (int j = 0; j < noOfBits; j++)
        if (codes[i][j] == 0)
          g->FillEllipse(&whiteBrush,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].x -
                             codeCircleDiameterSize / 2,
                         innerCircleTopLeft +
                             innerCircleDiameterSize * (float)codeLocs[j].y -
                             codeCircleDiameterSize / 2,
                         codeCircleDiameterSize, codeCircleDiameterSize);

      
      
      
      
      Bitmap *ringImg = new Bitmap(fileSize, fileSize, PixelFormat32bppARGB);
      Graphics *gRing = Graphics::FromImage(ringImg);
      gRing->SetSmoothingMode(SmoothingModeHighQuality);
      gRing->FillRectangle(&blackBrush, 0, 0, fileSize, fileSize);
      gRing->FillEllipse(&whiteBrush, outerCircleTopLeft, outerCircleTopLeft,
                         outerCircleDiameterSize, outerCircleDiameterSize);
      gRing->SetSmoothingMode(SmoothingModeNone);
      gRing->FillEllipse(&blackBrush, innerCircleTopLeft - 1,
                         innerCircleTopLeft - 1, innerCircleDiameterSize + 2,
                         innerCircleDiameterSize + 2);
      delete gRing;

      Color makeTrans;
      makeTrans.SetValue(Color::Black);
      ImageAttributes imageAtt;
      imageAtt.SetColorKey(makeTrans, makeTrans, ColorAdjustTypeDefault);
      g->DrawImage(ringImg, Rect(0, 0, fileSize, fileSize), 0, 0, fileSize,
                   fileSize, UnitPixel, &imageAtt);
      delete ringImg;

      float hdCenterPos = (float)(1.875 * borderSize);
      float numberCenterPos = (float)(2.375 * borderSize);

      
      g->SetTextRenderingHint(TextRenderingHintAntiAlias);
      
      float centerString = (float)(2.375 * borderSize);
      g->TranslateTransform(centerString, centerString);
      g->RotateTransform(315);
      std::wstring idxStr = createIndexString(i);
      RectF layoutRect;
      g->MeasureString(idxStr.c_str(), -1, largeFont, PointF(0, 0),
                       &layoutRect);
      g->DrawString(idxStr.c_str(), -1, largeFont,
                    PointF(-(layoutRect.Width / 2), -(layoutRect.Height / 2)),
                    &whiteBrush);
      g->ResetTransform();

      
      centerString = (float)(1.875 * borderSize);
      g->TranslateTransform(centerString, centerString);
      g->RotateTransform(315);
      std::wstring hdStr = StringToWString("HD" + std::to_string(HD));
      g->MeasureString(hdStr.c_str(), -1, smallFont, PointF(0, 0), &layoutRect);
      g->DrawString(hdStr.c_str(), -1, smallFont,
                    PointF(-(layoutRect.Width / 2), -(layoutRect.Height / 2)),
                    &whiteBrush);
      g->ResetTransform();

      
      std::string idStrA = std::to_string(i);
      while (idStrA.length() < 5)
        idStrA = "0" + idStrA;
      std::string pathStr = dirName + "/" + idStrA + ".png";
      std::wstring pathW = StringToWString(pathStr);
      img->Save(pathW.c_str(), &pngClsid, NULL);
      std::cout << "Saved: " << pathStr << std::endl;

      delete g;
      delete img;
    }
  }
};

/**
 * @brief 程序入口，执行 Windows 平台 STag 标记批量生成流程。
 * @return 成功返回 0。
 */
int main() {
  
  SetConsoleOutputCP(CP_UTF8);

  
  GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  EDMarkerGenerator generator;
  generator.run();

  
  GdiplusShutdown(gdiplusToken);
  return 0;
}
