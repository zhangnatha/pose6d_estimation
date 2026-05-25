# ArUco Module

`aruco/` 是根工程中的 ArUco 功能模块，提供：

- ArUco 制码
- 相机内参标定
- ArUco 位姿估计

当前模块不支持单独编译，必须从仓库根目录 `pose6d_estimation/` 统一构建。

## 目录结构

```shell
aruco/
├── CMakeLists.txt
├── README.md
├── assets/
├── calib/
├── generator/
└── pose/
```

## 公共依赖

本模块依赖根目录共享资源：

- `../3rdparty/opencv`
- `../3rdparty/realsense`
- `../3rdparty/orbbec`
- `../common/camera`

依赖安装脚本统一放在根目录 `../scripts/`。

## 编译

```bash
cd pose6d_estimation
mkdir -p build
cd build
cmake ..
make -j4
```

如只构建 ArUco：

```bash
cmake .. -DBUILD_ARUCO_TOOLS=ON -DBUILD_STAG_TOOLS=OFF
make -j4
```

## 运行

```bash
cd build/aruco/generator
./arucoMarkerGen

cd ../calib
./arucoCalib

cd ../pose
./arucoMarkerPose
./arucoMarkerPose ./config.yaml
./arucoMarkerPose ./sample.png ./intrinsic_calib.yaml
```

## 位姿配置

`pose/config.yaml` 常用字段：

| 字段 | 说明 |
| --- | --- |
| `mode` | `camera` 或 `image` |
| `image_path` | 单图模式输入图片 |
| `camera_type` | `realsense` 或 `orbbec` |
| `intrinsics_source` | `sdk` 或 `yaml` |
| `calib_yaml_path` | 标定文件路径 |
| `marker_size_m` | 实际边长，单位 m |
| `dictionary` | ArUco 字典 |
| `camera_width/height/fps` | 采集参数 |

## 标定配置

`arucoCalib` 也复用公共相机层，支持：

- `camera_type: realsense`
- `camera_type: orbbec`

输出的 `intrinsic_calib.yaml` 可直接供位姿模块使用。
