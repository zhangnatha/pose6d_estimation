cmake_minimum_required(VERSION 3.10)
project(Aruco_Pose)

# ArUco 工具链包含生成、标定与位姿三个子模块。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(generator)
add_subdirectory(calib)
add_subdirectory(pose)
