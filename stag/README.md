# STag Module

`stag/` 是根工程中的 STag 功能模块，提供：

- STag 制码
- STag 位姿估计

当前模块不支持单独编译，必须从仓库根目录 `pose6d_estimation/` 统一构建。

## 目录结构

```shell
stag/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── generator/
└── pose/
```

## 公共依赖

本模块依赖根目录共享资源：

- `../3rdparty/opencv`
- `../3rdparty/realsense`
- `../3rdparty/orbbec`
- `../3rdparty/stag`
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

如只构建 STag：

```bash
cmake .. -DBUILD_ARUCO_TOOLS=OFF -DBUILD_STAG_TOOLS=ON
make -j4
```

## 运行

```bash
cd build/stag/generator
./stagMarkerGen

cd ../pose
./stagMarkerPose
./stagMarkerPose ./config.yaml
./stagMarkerPose ./sample.png ./intrinsic_calib.yaml
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
| `library_hd` | `11/13/15/17/19/21/23` |
| `camera_width/height/fps` | 采集参数 |

## 说明

- `library_hd` 必须与实际打印的 STag 码族一致。
- `marker_size_m` 必须与真实物理边长一致，否则平移量会按比例偏差。
- 相机模式支持 `realsense` / `orbbec`，内参支持 `sdk` / `yaml`。
