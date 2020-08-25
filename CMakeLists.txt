# CMake 最低版本号要求

cmake_minimum_required (VERSION 2.8)

# 项目信息

project (example)

# 指定生成目标

add_executable(example example.c AVLTree.c FileDatabase.c)
