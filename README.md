# 🎮 GPU 性能测试 (GpuBench)

一个 Android 原生图形性能测试应用，用于对比 OpenGL ES 和 Vulkan 的性能差异。

## 功能特性

### 测试项目

1. **三角形渲染测试**
   - 实例化渲染 (Instanced Rendering)
   - 10,000 个 PBR 着色球体
   - 测试顶点吞吐和 Draw Call 效率

2. **复杂着色器测试**
   - Mandelbulb 分形光线步进
   - 复杂片段着色器计算
   - 测试着色器执行效率

3. **计算着色器测试**
   - 100 万粒子模拟
   - Simplex 噪声驱动
   - 测试 GPU 通用计算能力

4. **纹理采样测试**
   - 4 层 2048x2048 纹理
   - 各向异性过滤
   - 测试纹理带宽和采样效率

### 使用的现代特性

#### OpenGL ES 3.2+
- ✅ 计算着色器 (Compute Shaders)
- ✅ 实例化渲染 (Instanced Rendering)
- ✅ 着色器存储缓冲区 (SSBO)
- ✅ 各向异性过滤
- ✅ Mipmap
- ✅ PBR 着色器

#### Vulkan 1.3
- ✅ 动态渲染 (Dynamic Rendering)
- ✅ 同步2 (Synchronization2)
- ✅ 子组操作 (Subgroup Operations)
- ✅ 描述符索引 (Descriptor Indexing)
- ✅ 缓冲区设备地址 (Buffer Device Address)
- ✅ 推送描述符 (Push Descriptors)
- ✅ 计算着色器
- ✅ 间接绘制 (Indirect Draw)

## 构建要求

- Android Studio Arctic Fox 或更高版本
- Android NDK 25 或更高版本
- CMake 3.22.1 或更高版本
- Android SDK 34
- 最低支持 Android 7.0 (API 24)

## 构建步骤

1. 用 Android Studio 打开项目
2. 等待 Gradle 同步完成
3. 连接 Android 设备或启动模拟器
4. 点击 Run 运行

## 输出结果

测试结果包括：
- **FPS**: 每秒帧数
- **帧时间**: 每帧渲染时间 (ms)
- **三角形数**: 每帧渲染的三角形数量
- **Draw Calls**: 每帧的绘制调用次数

## 项目结构

```
GpuBench/
├── app/
│   ├── src/main/
│   │   ├── java/com/gpubench/      # Java UI 层
│   │   ├── cpp/                     # C++ 原生代码
│   │   │   ├── common/              # 通用工具
│   │   │   ├── gl/                  # OpenGL ES 引擎
│   │   │   └── vulkan/              # Vulkan 引擎
│   │   └── res/                     # 资源文件
│   └── build.gradle
├── build.gradle
└── settings.gradle
```

## 注意事项

1. **Vulkan 着色器**: Vulkan 测试需要预编译的 SPIR-V 着色器。当前版本使用占位实现，需要使用 `glslangValidator` 编译 GLSL 到 SPIR-V。

2. **设备兼容性**: 不是所有设备都支持 Vulkan。应用会自动检测并降级到 OpenGL ES。

3. **性能对比**: 为了公平对比，两个 API 使用相同的测试场景和渲染逻辑。

## 许可证

MIT License
