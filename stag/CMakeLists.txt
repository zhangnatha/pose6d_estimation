cmake_minimum_required(VERSION 3.10)
project(STag_Pose)

# STag 模块拆分为标记生成与位姿估计两个子模块。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(generator)
add_subdirectory(pose)
