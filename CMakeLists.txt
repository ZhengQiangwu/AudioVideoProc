cmake_minimum_required(VERSION 3.10)
project(AudioVideoProc CXX)
set(CMAKE_BUILD_TYPE Debug) # 设置构建类型为Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g") # 确保 -g 标志被添加

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# 查找依赖包
# ----------------------------------------------------------------------------
# 添加线程支持
# 查找线程库 (pthreads)
find_package(Threads REQUIRED)

# 查找X11库
find_package(X11 REQUIRED)

# 查找OpenCV
find_package(OpenCV 4.9.0 REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV version: ${OpenCV_VERSION}")
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()

# ----------------------------------------------------------------------------
# 定义项目包含目录
# ----------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    /opt/ffmpeg-static/include
    /home/awu666/lib/libyuv-main/include
)

# ----------------------------------------------------------------------------
# 定义源文件和库目标
# ----------------------------------------------------------------------------
# Source files
set(SOURCE_FILES
    AudioVideoProc.cpp
    AudioVideoProcModule.cpp
    Log.cpp
    MediaFrameCapture.cpp
    Tool.cpp
    VideoCapManager.cpp
)

# Header files (for reference, not directly added to target)
set(HEADER_FILES
    AudioVideoProc.h
    AudioVideoProcModule.h
    Log.h
    MediaFrameCapture.h
    Tool.h
    VideoCapManager.h
)

set(OpenCV_LIBS 
    "/usr/local/lib/libopencv_world.so.4.9.0"
)

# 使用 add_library 创建一个名为 AudioVideoProc 的共享库 (.so 文件)
add_library(AudioVideoProc SHARED ${SOURCE_FILES})

# ----------------------------------------------------------------------------
# 链接依赖库到目标
# ----------------------------------------------------------------------------
# 使用 link_directories 告诉链接器去哪里找 FFmpeg 和 libyuv 的库文件
# link_directories(
#     /opt/ffmpeg/lib
    # --- 修改开始: 使用 libyuv.a 所在的正确目录 ---
#     /home/alientek/libyuv/libyuv-main/build
    # --- 修改结束 ---
# )

target_link_libraries(AudioVideoProc
    # 链接OpenCV
    # 使用 find_package 提供的 ${OpenCV_LIBS} 变量，这是正确的做法
    #${OpenCV_LIBS}

    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libopencv_world.so.409

    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libx264.so.160
    # 链接libyuv库
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libyuv.so
    # --- 关键修改: 链接FFmpeg的【静态库 .a】 ---
    # 链接顺序通常很重要，依赖最少的库放在最后
    /opt/ffmpeg-static/lib/libavdevice.a
    /opt/ffmpeg-static/lib/libavfilter.a
    /opt/ffmpeg-static/lib/libavformat.a
    /opt/ffmpeg-static/lib/libavcodec.a
    /opt/ffmpeg-static/lib/libswresample.a
    /opt/ffmpeg-static/lib/libswscale.a
    /opt/ffmpeg-static/lib/libpostproc.a
    /opt/ffmpeg-static/lib/libavutil.a

    /opt/deps/openh264/lib/libopenh264.a
    # --- 关键修改: 手动链接FFmpeg静态库所需要的所有底层依赖 ---
    # 你需要根据FFmpeg编译时的依赖来添加这些
    # 链接其他系统库
    Threads::Threads
    X11::X11
    pulse
    asound
    bz2
    z
    lzma
    dl
    m

    # 链接FFmpeg库
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavdevice.so.58
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavfilter.so.7
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavformat.so.58
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavcodec.so.58
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libswresample.so.3
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libswscale.so.5
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavutil.so.56

    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavdevice.so.58.8.100
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavfilter.so.7.57.100
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavformat.so.58.29.100
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavcodec.so.58.54.100
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libswresample.so.3.5.100
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libswscale.so.5.5.100
    #${CMAKE_CURRENT_SOURCE_DIR}/libs/libavutil.so.56.31.100
    #avcodec
    #avdevice
    #avfilter
    #avformat
    #avutil
    #swresample
    #swscale
)
# ============================================================================
# 设置 RPATH (运行时搜索路径)
# ============================================================================

# 设置最终安装后的 RPATH
# $ORIGIN 是一个特殊的链接器变量，代表“库文件自己所在的目录”
# 这使得我们的库无论被移动到哪里，都能在自己的旁边找到其他依赖
# 设置构建时的 RPATH
set_target_properties(AudioVideoProc PROPERTIES
    BUILD_RPATH "$ORIGIN"
    #BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/libs"
)
# ----------------------------------------------------------------------------
# 符号可见性与链接器选项（关键：避免 Electron 自带 ffmpeg 串味）
# ----------------------------------------------------------------------------
# 说明：
# - 你的头文件里已经对所有对外 API 使用了
#   #define AUDIOVIDEOPROC_API __attribute__((visibility("default")))
#   且在 extern "C" 作用域下，这样就不需要 version-script 了。
# - 保留默认隐藏与排除静态库符号，避免 FFmpeg 符号外泄/互相劫持。
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

target_link_options(AudioVideoProc PRIVATE
    -Wl,--exclude-libs,ALL
    -Wl,-Bsymbolic
    -Wl,-Bsymbolic-functions
    # 如果你的发行版默认 --as-needed 导致某些静态库被丢弃，
    # 可临时加上下一行（通常不需要）：
    # -Wl,--no-as-needed
)