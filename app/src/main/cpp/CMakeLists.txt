cmake_minimum_required(VERSION 3.22.1)
project("gpubench")

# C++17 标准
set(CMAKE_CXX_STANDARD 17)

# 源文件
set(COMMON_SOURCES
    common/Benchmark.cpp
    common/ShaderUtils.cpp
    common/Timer.cpp
    common/MathUtils.cpp
)

set(GL_SOURCES
    gl/GLEngine.cpp
)

set(VK_SOURCES
    vulkan/VulkanEngine.cpp
    vulkan/VulkanDevice.cpp
    vulkan/VulkanSwapchain.cpp
    vulkan/VulkanPipeline.cpp
    vulkan/VulkanTriangleTest.cpp
    vulkan/VulkanShaderTest.cpp
    vulkan/VulkanComputeTest.cpp
    vulkan/VulkanTextureTest.cpp
)

# 主库
add_library(gpubench SHARED
    native-lib.cpp
    ${COMMON_SOURCES}
    ${GL_SOURCES}
    ${VK_SOURCES}
)

# 头文件目录
target_include_directories(gpubench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/gl
    ${CMAKE_SOURCE_DIR}/vulkan
)

# 链接库
find_library(log-lib log)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)

target_link_libraries(gpubench
    ${log-lib}
    ${android-lib}
    ${jnigraphics-lib}
    EGL
    GLESv3
    vulkan
)

# 编译优化
target_compile_options(gpubench PRIVATE
    -O3
    -ffast-math
    -funroll-loops
)
