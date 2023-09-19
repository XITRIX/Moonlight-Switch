set -e

# ========  手动构建 libromfs-generator ========
# 本项目支持使用 libromfs 将主程序和资源文件打包在一起，避免额外的文件依赖
# libromfs-generator 的作用是将资源文件转换为 cpp 源码，并在后续的编译过程中链接进主程序
# 在跨平台编译时，有两种解决方案：
# 1. 提前编译host机器格式的 libromfs-generator
# 2. 设置 CMAKE_CROSSCOMPILING_EMULATOR 以在 host 模拟运行 libromfs-generator
# 此脚本即用来生成 host机器格式的 libromfs-generator

# ======== Manually building libromfs generator ========
# This project supports using libromfs to package the main program and resource files together to avoid additional file dependencies
# The function of libromfs-generator is to convert resource files into cpp source code and link them into the main program
# There are two solutions for cross-platform compilation:
# 1. Pre-compile libromfs-generator in the format of the host machine.
# 2. Set CMAKE_CROSSCOMPILING_EMULATOR to simulate execution libromfs-generator on the host.
# This script is used to generate libromfs-generator in the format of the host machine.

echo "Build libromfs-generator"

PROJECT_PATH=$(dirname "$0")
LIBROMFS_PATH="${PROJECT_PATH}/extern/borealis/library/lib/extern/libromfs/generator"
BUILD_DIR="build_libromfs_generator"

cd "${PROJECT_PATH}"

# build libromfs-generator
cmake -B ${BUILD_DIR} "${LIBROMFS_PATH}"
make -C ${BUILD_DIR}

# put libromfs-generator under the jni folder
cp ${BUILD_DIR}/libromfs-generator "${PROJECT_PATH}"
echo "Build libromfs-generator: ${PROJECT_PATH}/libromfs-generator"

# remove build folder
rm -rf ${BUILD_DIR}
echo "Remove temp build dir: ${BUILD_DIR}"
