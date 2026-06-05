#include "GLEngine.h"

namespace GpuBench {

GLEngine::GLEngine() = default;

GLEngine::~GLEngine() {
    shutdown();
}

bool GLEngine::initialize(void* surface, int width, int height) {
    if (initialized_) return true;

    window_ = static_cast<ANativeWindow*>(surface);
    width_ = width;
    height_ = height;

    if (!initEGL()) {
        LOGE("Failed to initialize EGL");
        return false;
    }

    printGLInfo();
    detectCapabilities();

    // 设置视口
    glViewport(0, 0, width_, height_);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    initialized_ = true;
    LOGI("OpenGL ES engine initialized: %dx%d", width_, height_);
    return true;
}

void GLEngine::shutdown() {
    if (!initialized_) return;
    shutdownEGL();
    initialized_ = false;
    LOGI("OpenGL ES engine shut down");
}

bool GLEngine::initEGL() {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) {
        LOGE("eglInitialize failed");
        return false;
    }
    LOGI("EGL version: %d.%d", major, minor);

    // EGL 配置 - 请求 OpenGL ES 3.2
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display_, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    // 创建窗口表面
    surface_ = eglCreateWindowSurface(display_, config, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed");
        return false;
    }

    // 创建 OpenGL ES 3.2 上下文
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };

    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed, trying 3.0");
        const EGLint contextAttribs30[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
        };
        context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs30);
        if (context_ == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed");
            return false;
        }
    }

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOGE("eglMakeCurrent failed");
        return false;
    }

    return true;
}

void GLEngine::shutdownEGL() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void GLEngine::printGLInfo() {
    LOGI("GL_VENDOR: %s", glGetString(GL_VENDOR));
    LOGI("GL_RENDERER: %s", glGetString(GL_RENDERER));
    LOGI("GL_VERSION: %s", glGetString(GL_VERSION));
    LOGI("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    LOGI("GL_EXTENSIONS: %s", glGetString(GL_EXTENSIONS));
}

void GLEngine::detectCapabilities() {
    // 计算着色器
    capabilities_.computeShader = true; // ES 3.1+

    // 获取计算着色器限制
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &capabilities_.maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &capabilities_.maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &capabilities_.maxComputeWorkGroupCount[2]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &capabilities_.maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &capabilities_.maxComputeWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &capabilities_.maxComputeWorkGroupSize[2]);
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &capabilities_.maxComputeWorkGroupInvocations);

    // 纹理大小
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &capabilities_.maxTextureSize);

    // 其他特性
    capabilities_.instancedRendering = true; // ES 3.0+
    capabilities_.uniformBufferObject = true; // ES 3.0+
    capabilities_.transformFeedback = true; // ES 3.0+
    capabilities_.seamlessCubemap = true; // ES 3.2

    // 检查扩展
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (extensions) {
        capabilities_.shaderStorageBuffer = strstr(extensions, "GL_EXT_shader_storage_buffer_object") != nullptr;
        capabilities_.imageLoadStore = strstr(extensions, "GL_EXT_shader_image_load_store") != nullptr;
        capabilities_.atomicCounters = strstr(extensions, "GL_EXT_shader_atomic_counters") != nullptr;
    }

    LOGI("Capabilities:");
    LOGI("  Compute Shader: %s", capabilities_.computeShader ? "YES" : "NO");
    LOGI("  Max Texture Size: %d", capabilities_.maxTextureSize);
    LOGI("  Max Compute Work Group Count: %d x %d x %d",
         capabilities_.maxComputeWorkGroupCount[0],
         capabilities_.maxComputeWorkGroupCount[1],
         capabilities_.maxComputeWorkGroupCount[2]);
    LOGI("  Max Compute Work Group Size: %d x %d x %d",
         capabilities_.maxComputeWorkGroupSize[0],
         capabilities_.maxComputeWorkGroupSize[1],
         capabilities_.maxComputeWorkGroupSize[2]);
}

void GLEngine::checkGLError(const std::string& op) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGE("GL error after %s: 0x%x", op.c_str(), error);
    }
}

std::vector<std::string> GLEngine::getTestNames() const {
    return {
        "Triangle Rendering",
        "Complex Shaders",
        "Compute Shaders",
        "Texture Sampling"
    };
}

TestResult GLEngine::run(const std::string& testName, int frameCount) {
    if (testName == "Triangle Rendering") return runTriangleTest(frameCount);
    if (testName == "Complex Shaders") return runShaderTest(frameCount);
    if (testName == "Compute Shaders") return runComputeTest(frameCount);
    if (testName == "Texture Sampling") return runTextureTest(frameCount);

    LOGE("Unknown test: %s", testName.c_str());
    return {};
}

// ============= 三角形渲染测试 =============
TestResult GLEngine::runTriangleTest(int frameCount) {
    LOGI("Running GL Triangle Test...");

    // 顶点着色器 - 使用实例化渲染
    const std::string vertexShader = R"(#version 320 es
        precision highp float;

        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;

        uniform mat4 uProjection;
        uniform mat4 uView;

        // 实例数据
        layout(location = 3) in vec3 aInstancePos;
        layout(location = 4) in vec4 aInstanceColor;
        layout(location = 5) in float aInstanceScale;

        out vec3 vNormal;
        out vec3 vWorldPos;
        out vec2 vTexCoord;
        out vec4 vColor;

        void main() {
            vec3 worldPos = aPosition * aInstanceScale + aInstancePos;
            gl_Position = uProjection * uView * vec4(worldPos, 1.0);
            vNormal = aNormal;
            vWorldPos = worldPos;
            vTexCoord = aTexCoord;
            vColor = aInstanceColor;
        }
    )";

    // 片段着色器 - PBR 风格
    const std::string fragmentShader = R"(#version 320 es
        precision highp float;

        in vec3 vNormal;
        in vec3 vWorldPos;
        in vec2 vTexCoord;
        in vec4 vColor;

        layout(location = 0) out vec4 fragColor;

        uniform vec3 uLightPos;
        uniform vec3 uCameraPos;

        void main() {
            vec3 N = normalize(vNormal);
            vec3 L = normalize(uLightPos - vWorldPos);
            vec3 V = normalize(uCameraPos - vWorldPos);
            vec3 H = normalize(L + V);

            // PBR 简化
            float NdotL = max(dot(N, L), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            float specular = pow(NdotH, 64.0);
            float ambient = 0.15;

            vec3 color = vColor.rgb * (ambient + NdotL * 0.7) + vec3(specular * 0.3);
            fragColor = vec4(color, vColor.a);
        }
    )";

    GLuint program = GLShaderUtils::createProgram(vertexShader, fragmentShader);
    if (!program) {
        LOGE("Failed to create triangle shader program");
        return {};
    }

    // 生成球体几何数据
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    generateSphere(32, 32, vertices, indices);

    // 创建 VAO/VBO/EBO
    GLuint VAO, VBO, EBO, instanceVBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(VAO);

    // 顶点数据
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // 顶点属性
    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texcoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // 实例数据 - 10000 个球体
    const int INSTANCE_COUNT = 10000;
    struct InstanceData {
        Vec3 position;
        Vec4 color;
        float scale;
    };
    std::vector<InstanceData> instances(INSTANCE_COUNT);
    srand(42);
    for (int i = 0; i < INSTANCE_COUNT; i++) {
        float x = (rand() % 200 - 100) / 10.0f;
        float y = (rand() % 200 - 100) / 10.0f;
        float z = (rand() % 200 - 100) / 10.0f;
        instances[i].position = {x, y, z};
        float r = (rand() % 100) / 100.0f;
        float g = (rand() % 100) / 100.0f;
        float b = (rand() % 100) / 100.0f;
        instances[i].color = {r, g, b, 1.0f};
        instances[i].scale = 0.05f + (rand() % 100) / 500.0f;
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_STATIC_DRAW);

    // 实例属性 - 位置
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);
    // 颜色
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)sizeof(Vec3));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);
    // 缩放
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(sizeof(Vec3) + sizeof(Vec4)));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);

    // 矩阵
    Mat4 projection = Mat4::perspective(1.0472f, (float)width_ / height_, 0.1f, 200.0f);
    Mat4 view = Mat4::lookAt({0, 0, 50}, {0, 0, 0}, {0, 1, 0});

    // 运行测试
    FrameTimer timer;
    uint64_t totalTriangles = 0;

    // 预热
    for (int i = 0; i < config_.warmupFrames; i++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, projection.m);
        glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, view.m);
        glUniform3f(glGetUniformLocation(program, "uLightPos"), 20.0f, 20.0f, 20.0f);
        glUniform3f(glGetUniformLocation(program, "uCameraPos"), 0.0f, 0.0f, 50.0f);

        glBindVertexArray(VAO);
        glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr, INSTANCE_COUNT);

        eglSwapBuffers(display_, surface_);
    }

    // 正式测试
    for (int i = 0; i < frameCount; i++) {
        timer.beginFrame();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, projection.m);
        glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, view.m);
        glUniform3f(glGetUniformLocation(program, "uLightPos"), 20.0f, 20.0f, 20.0f);
        glUniform3f(glGetUniformLocation(program, "uCameraPos"), 0.0f, 0.0f, 50.0f);

        glBindVertexArray(VAO);
        glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr, INSTANCE_COUNT);

        eglSwapBuffers(display_, surface_);

        timer.endFrame();
        totalTriangles += indices.size() / 3 * INSTANCE_COUNT;
    }

    // 清理
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &instanceVBO);
    glDeleteProgram(program);

    TestResult result;
    result.name = "Triangle Rendering";
    result.api = getApiName();
    result.fps = timer.getAverageFps();
    result.frameTimeMs = timer.getAverageMs();
    result.triangles = totalTriangles / frameCount;
    result.drawCalls = 1; // 实例化渲染只有 1 次 draw call
    return result;
}

// ============= 复杂着色器测试 =============
TestResult GLEngine::runShaderTest(int frameCount) {
    LOGI("Running GL Shader Test...");

    // 复杂片段着色器 - 光线步进 + 分形
    const std::string vertexShader = R"(#version 320 es
        precision highp float;
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 vTexCoord;
        void main() {
            gl_Position = vec4(aPosition, 1.0);
            vTexCoord = aTexCoord;
        }
    )";

    const std::string fragmentShader = R"(#version 320 es
        precision highp float;
        in vec2 vTexCoord;
        layout(location = 0) out vec4 fragColor;
        uniform float uTime;
        uniform vec2 uResolution;

        // Mandelbulb 分形
        float mandelbulb(vec3 p) {
            vec3 z = p;
            float dr = 1.0;
            float r = 0.0;
            float power = 8.0;
            int iterations = 0;

            for (int i = 0; i < 12; i++) {
                r = length(z);
                if (r > 2.0) break;
                iterations++;

                float theta = acos(z.z / r);
                float phi = atan(z.y, z.x);
                float zr = pow(r, power);
                dr = pow(r, power - 1.0) * power * dr + 1.0;

                theta *= power;
                phi *= power;

                z = zr * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
                z += p;
            }
            return 0.5 * log(r) * r / dr;
        }

        // 场景距离
        float map(vec3 p) {
            return mandelbulb(p);
        }

        // 法线计算
        vec3 calcNormal(vec3 p) {
            vec2 e = vec2(0.001, 0.0);
            return normalize(vec3(
                map(p + e.xyy) - map(p - e.xyy),
                map(p + e.yxy) - map(p - e.yxy),
                map(p + e.yyx) - map(p - e.yyx)
            ));
        }

        // 光线步进
        vec4 raymarch(vec3 ro, vec3 rd) {
            float t = 0.0;
            float maxDist = 10.0;

            for (int i = 0; i < 100; i++) {
                vec3 p = ro + rd * t;
                float d = map(p);

                if (d < 0.001) {
                    vec3 n = calcNormal(p);
                    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
                    float diff = max(dot(n, lightDir), 0.0);
                    float spec = pow(max(dot(reflect(-lightDir, n), -rd), 0.0), 32.0);

                    // 根据迭代次数着色
                    float ao = 1.0 - float(i) / 100.0;
                    vec3 color = mix(vec3(0.2, 0.5, 1.0), vec3(1.0, 0.3, 0.1), ao);
                    color = color * (0.3 + diff * 0.5) + vec3(spec * 0.2);

                    return vec4(color, 1.0);
                }

                t += d;
                if (t > maxDist) break;
            }

            // 背景色
            vec3 bg = mix(vec3(0.05, 0.05, 0.1), vec3(0.1, 0.15, 0.2), vTexCoord.y);
            return vec4(bg, 1.0);
        }

        void main() {
            vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);

            // 相机
            float angle = uTime * 0.3;
            vec3 ro = vec3(3.0 * sin(angle), 2.0, 3.0 * cos(angle));
            vec3 target = vec3(0.0, 0.0, 0.0);
            vec3 forward = normalize(target - ro);
            vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
            vec3 up = cross(right, forward);
            vec3 rd = normalize(forward + uv.x * right + uv.y * up);

            vec4 color = raymarch(ro, rd);

            // Gamma 校正
            color.rgb = pow(color.rgb, vec3(1.0 / 2.2));
            fragColor = color;
        }
    )";

    GLuint program = GLShaderUtils::createProgram(vertexShader, fragmentShader);
    if (!program) return {};

    // 全屏四边形
    float quadVertices[] = {
        -1, -1, 0, 0, 0,
         1, -1, 0, 1, 0,
         1,  1, 0, 1, 1,
        -1,  1, 0, 0, 1,
    };
    uint16_t quadIndices[] = {0, 1, 2, 0, 2, 3};

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    FrameTimer timer;
    float time = 0.0f;

    // 预热
    for (int i = 0; i < config_.warmupFrames; i++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "uTime"), time);
        glUniform2f(glGetUniformLocation(program, "uResolution"), width_, height_);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        eglSwapBuffers(display_, surface_);
        time += 0.016f;
    }

    // 正式测试
    for (int i = 0; i < frameCount; i++) {
        timer.beginFrame();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "uTime"), time);
        glUniform2f(glGetUniformLocation(program, "uResolution"), width_, height_);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        eglSwapBuffers(display_, surface_);

        timer.endFrame();
        time += 0.016f;
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(program);

    TestResult result;
    result.name = "Complex Shaders";
    result.api = getApiName();
    result.fps = timer.getAverageFps();
    result.frameTimeMs = timer.getAverageMs();
    result.triangles = 2; // 全屏四边形
    result.drawCalls = 1;
    return result;
}

// ============= 计算着色器测试 =============
TestResult GLEngine::runComputeTest(int frameCount) {
    LOGI("Running GL Compute Test...");

    // 粒子模拟计算着色器
    const std::string computeShader = R"(#version 320 es
        precision highp float;

        layout(local_size_x = 256) in;

        struct Particle {
            vec4 position;
            vec4 velocity;
            vec4 color;
        };

        layout(std430, binding = 0) buffer ParticleBuffer {
            Particle particles[];
        };

        uniform float uTime;
        uniform float uDeltaTime;
        uniform uint uParticleCount;

        // 噪声函数
        vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
        vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
        vec4 permute(vec4 x) { return mod289(((x * 34.0) + 1.0) * x); }
        vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

        float snoise(vec3 v) {
            const vec2 C = vec2(1.0/6.0, 1.0/3.0);
            const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
            vec3 i = floor(v + dot(v, C.yyy));
            vec3 x0 = v - i + dot(i, C.xxx);
            vec3 g = step(x0.yzx, x0.xyz);
            vec3 l = 1.0 - g;
            vec3 i1 = min(g.xyz, l.zxy);
            vec3 i2 = max(g.xyz, l.zxy);
            vec3 x1 = x0 - i1 + C.xxx;
            vec3 x2 = x0 - i2 + C.yyy;
            vec3 x3 = x0 - D.yyy;
            i = mod289(i);
            vec4 p = permute(permute(permute(
                i.z + vec4(0.0, i1.z, i2.z, 1.0))
                + i.y + vec4(0.0, i1.y, i2.y, 1.0))
                + i.x + vec4(0.0, i1.x, i2.x, 1.0));
            float n_ = 0.142857142857;
            vec3 ns = n_ * D.wyz - D.xzx;
            vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
            vec4 x_ = floor(j * ns.z);
            vec4 y_ = floor(j - 7.0 * x_);
            vec4 x = x_ * ns.x + ns.yyyy;
            vec4 y = y_ * ns.x + ns.yyyy;
            vec4 h = 1.0 - abs(x) - abs(y);
            vec4 b0 = vec4(x.xy, y.xy);
            vec4 b1 = vec4(x.zw, y.zw);
            vec4 s0 = floor(b0) * 2.0 + 1.0;
            vec4 s1 = floor(b1) * 2.0 + 1.0;
            vec4 sh = -step(h, vec4(0.0));
            vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
            vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
            vec3 p0 = vec3(a0.xy, h.x);
            vec3 p1 = vec3(a0.zw, h.y);
            vec3 p2 = vec3(a1.xy, h.z);
            vec3 p3 = vec3(a1.zw, h.w);
            vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2,p2), dot(p3,p3)));
            p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
            vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
            m = m * m;
            return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
        }

        void main() {
            uint id = gl_GlobalInvocationID.x;
            if (id >= uParticleCount) return;

            Particle p = particles[id];

            // 使用噪声驱动粒子运动
            vec3 noiseInput = p.position.xyz * 0.5 + uTime * 0.2;
            vec3 force = vec3(
                snoise(noiseInput),
                snoise(noiseInput + vec3(100.0, 0.0, 0.0)),
                snoise(noiseInput + vec3(0.0, 100.0, 0.0))
            ) * 2.0;

            // 引力中心
            vec3 toCenter = -p.position.xyz;
            float dist = length(toCenter);
            force += normalize(toCenter) * (1.0 / (dist * dist + 0.1)) * 0.5;

            // 更新速度和位置
            p.velocity.xyz += force * uDeltaTime;
            p.velocity.xyz *= 0.99; // 阻尼
            p.position.xyz += p.velocity.xyz * uDeltaTime;

            // 边界反弹
            if (length(p.position.xyz) > 10.0) {
                p.position.xyz = normalize(p.position.xyz) * 10.0;
                p.velocity.xyz *= -0.5;
            }

            // 更新颜色
            float speed = length(p.velocity.xyz);
            p.color = mix(vec4(0.2, 0.5, 1.0, 1.0), vec4(1.0, 0.3, 0.1, 1.0), speed / 5.0);

            particles[id] = p;
        }
    )";

    // 粒子渲染着色器
    const std::string particleVS = R"(#version 320 es
        precision highp float;
        layout(location = 0) in vec4 aPosition;
        layout(location = 1) in vec4 aVelocity;
        layout(location = 2) in vec4 aColor;
        uniform mat4 uMVP;
        out vec4 vColor;
        void main() {
            gl_Position = uMVP * vec4(aPosition.xyz, 1.0);
            gl_PointSize = max(2.0, 4.0 - length(aVelocity.xyz) * 0.3);
            vColor = aColor;
        }
    )";

    const std::string particleFS = R"(#version 320 es
        precision highp float;
        in vec4 vColor;
        layout(location = 0) out vec4 fragColor;
        void main() {
            vec2 coord = gl_PointCoord - vec2(0.5);
            float r = dot(coord, coord);
            if (r > 0.25) discard;
            float alpha = smoothstep(0.25, 0.1, r);
            fragColor = vec4(vColor.rgb, vColor.a * alpha);
        }
    )";

    GLuint computeProgram = GLShaderUtils::createComputeProgram(computeShader);
    GLuint renderProgram = GLShaderUtils::createProgram(particleVS, particleFS);
    if (!computeProgram || !renderProgram) return {};

    // 创建粒子缓冲区
    const uint32_t PARTICLE_COUNT = 1000000; // 100 万粒子
    struct Particle {
        Vec4 position;
        Vec4 velocity;
        Vec4 color;
    };

    std::vector<Particle> particles(PARTICLE_COUNT);
    srand(42);
    for (uint32_t i = 0; i < PARTICLE_COUNT; i++) {
        float x = (rand() % 2000 - 1000) / 100.0f;
        float y = (rand() % 2000 - 1000) / 100.0f;
        float z = (rand() % 2000 - 1000) / 100.0f;
        particles[i].position = {x, y, z, 1.0f};
        particles[i].velocity = {0, 0, 0, 0};
        particles[i].color = {0.2f, 0.5f, 1.0f, 1.0f};
    }

    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    // 渲染 VAO
    GLuint renderVAO;
    glGenVertexArrays(1, &renderVAO);
    glBindVertexArray(renderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ssbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)sizeof(Vec4));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)(2 * sizeof(Vec4)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    Mat4 projection = Mat4::perspective(1.0472f, (float)width_ / height_, 0.1f, 100.0f);
    Mat4 view = Mat4::lookAt({0, 0, 30}, {0, 0, 0}, {0, 1, 0});
    Mat4 mvp = projection * view;

    FrameTimer timer;
    float time = 0.0f;
    uint32_t groupCount = (PARTICLE_COUNT + 255) / 256;

    // 预热
    for (int i = 0; i < config_.warmupFrames; i++) {
        // 计算
        glUseProgram(computeProgram);
        glUniform1f(glGetUniformLocation(computeProgram, "uTime"), time);
        glUniform1f(glGetUniformLocation(computeProgram, "uDeltaTime"), 0.016f);
        glUniform1ui(glGetUniformLocation(computeProgram, "uParticleCount"), PARTICLE_COUNT);
        glDispatchCompute(groupCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 渲染
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glUseProgram(renderProgram);
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "uMVP"), 1, GL_FALSE, mvp.m);
        glBindVertexArray(renderVAO);
        glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);

        eglSwapBuffers(display_, surface_);
        time += 0.016f;
    }

    // 正式测试
    for (int i = 0; i < frameCount; i++) {
        timer.beginFrame();

        // 计算
        glUseProgram(computeProgram);
        glUniform1f(glGetUniformLocation(computeProgram, "uTime"), time);
        glUniform1f(glGetUniformLocation(computeProgram, "uDeltaTime"), 0.016f);
        glUniform1ui(glGetUniformLocation(computeProgram, "uParticleCount"), PARTICLE_COUNT);
        glDispatchCompute(groupCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 渲染
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glUseProgram(renderProgram);
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "uMVP"), 1, GL_FALSE, mvp.m);
        glBindVertexArray(renderVAO);
        glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);

        eglSwapBuffers(display_, surface_);

        timer.endFrame();
        time += 0.016f;
    }

    glDeleteBuffers(1, &ssbo);
    glDeleteVertexArrays(1, &renderVAO);
    glDeleteProgram(computeProgram);
    glDeleteProgram(renderProgram);

    TestResult result;
    result.name = "Compute Shaders";
    result.api = getApiName();
    result.fps = timer.getAverageFps();
    result.frameTimeMs = timer.getAverageMs();
    result.triangles = PARTICLE_COUNT; // 粒子数
    result.drawCalls = 1;
    return result;
}

// ============= 纹理采样测试 =============
TestResult GLEngine::runTextureTest(int frameCount) {
    LOGI("Running GL Texture Test...");

    // 多纹理采样着色器
    const std::string vertexShader = R"(#version 320 es
        precision highp float;
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 vTexCoord;
        void main() {
            gl_Position = vec4(aPosition, 1.0);
            vTexCoord = aTexCoord;
        }
    )";

    const std::string fragmentShader = R"(#version 320 es
        precision highp float;
        in vec2 vTexCoord;
        layout(location = 0) out vec4 fragColor;

        uniform sampler2D uTexture0;
        uniform sampler2D uTexture1;
        uniform sampler2D uTexture2;
        uniform sampler2D uTexture3;
        uniform float uTime;

        void main() {
            vec2 uv = vTexCoord;

            // 多层纹理采样与混合
            vec4 c0 = texture(uTexture0, uv * 2.0 + uTime * 0.1);
            vec4 c1 = texture(uTexture1, uv * 3.0 - uTime * 0.15);
            vec4 c2 = texture(uTexture2, uv * 4.0 + vec2(uTime * 0.08, -uTime * 0.12));
            vec4 c3 = texture(uTexture3, uv * 5.0 - vec2(uTime * 0.05, uTime * 0.2));

            // 复杂混合
            float blend = sin(uTime * 0.5) * 0.5 + 0.5;
            vec4 color = mix(mix(c0, c1, blend), mix(c2, c3, 1.0 - blend), sin(uTime * 0.3) * 0.5 + 0.5);

            // 后处理效果
            float vignette = 1.0 - dot(uv - 0.5, uv - 0.5) * 2.0;
            color.rgb *= vignette;

            // 色调映射
            color.rgb = color.rgb / (color.rgb + vec3(1.0));

            fragColor = vec4(color.rgb, 1.0);
        }
    )";

    GLuint program = GLShaderUtils::createProgram(vertexShader, fragmentShader);
    if (!program) return {};

    // 创建测试纹理 (程序化生成)
    const int TEX_SIZE = 2048;
    std::vector<uint8_t> texData(TEX_SIZE * TEX_SIZE * 4);

    auto generateTexture = [&](int pattern) {
        for (int y = 0; y < TEX_SIZE; y++) {
            for (int x = 0; x < TEX_SIZE; x++) {
                int idx = (y * TEX_SIZE + x) * 4;
                float u = (float)x / TEX_SIZE;
                float v = (float)y / TEX_SIZE;

                switch (pattern) {
                    case 0: // 棋盘格
                        texData[idx] = ((int)(u * 16) + (int)(v * 16)) % 2 == 0 ? 255 : 0;
                        texData[idx+1] = texData[idx];
                        texData[idx+2] = texData[idx];
                        break;
                    case 1: // 渐变
                        texData[idx] = (uint8_t)(u * 255);
                        texData[idx+1] = (uint8_t)(v * 255);
                        texData[idx+2] = 128;
                        break;
                    case 2: // 噪声
                        {
                            float noise = sin(u * 50.0) * cos(v * 50.0) * 0.5 + 0.5;
                            texData[idx] = (uint8_t)(noise * 255);
                            texData[idx+1] = (uint8_t)(noise * 200);
                            texData[idx+2] = (uint8_t)(noise * 150);
                        }
                        break;
                    case 3: // 同心圆
                        {
                            float dx = u - 0.5f, dy = v - 0.5f;
                            float dist = sqrtf(dx*dx + dy*dy);
                            float ring = sin(dist * 50.0f) * 0.5f + 0.5f;
                            texData[idx] = (uint8_t)(ring * 255);
                            texData[idx+1] = (uint8_t)(ring * 180);
                            texData[idx+2] = (uint8_t)(ring * 100);
                        }
                        break;
                }
                texData[idx+3] = 255;
            }
        }
    };

    GLuint textures[4];
    glGenTextures(4, textures);
    for (int i = 0; i < 4; i++) {
        generateTexture(i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // 各向异性过滤
        glTexParameterf(GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */, 16.0f);
    }

    // 全屏四边形
    float quadVertices[] = {
        -1, -1, 0, 0, 0,
         1, -1, 0, 1, 0,
         1,  1, 0, 1, 1,
        -1,  1, 0, 0, 1,
    };
    uint16_t quadIndices[] = {0, 1, 2, 0, 2, 3};

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    FrameTimer timer;
    float time = 0.0f;

    // 预热
    for (int i = 0; i < config_.warmupFrames; i++) {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        for (int t = 0; t < 4; t++) {
            glActiveTexture(GL_TEXTURE0 + t);
            glBindTexture(GL_TEXTURE_2D, textures[t]);
        }
        glUniform1i(glGetUniformLocation(program, "uTexture0"), 0);
        glUniform1i(glGetUniformLocation(program, "uTexture1"), 1);
        glUniform1i(glGetUniformLocation(program, "uTexture2"), 2);
        glUniform1i(glGetUniformLocation(program, "uTexture3"), 3);
        glUniform1f(glGetUniformLocation(program, "uTime"), time);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        eglSwapBuffers(display_, surface_);
        time += 0.016f;
    }

    // 正式测试
    for (int i = 0; i < frameCount; i++) {
        timer.beginFrame();

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        for (int t = 0; t < 4; t++) {
            glActiveTexture(GL_TEXTURE0 + t);
            glBindTexture(GL_TEXTURE_2D, textures[t]);
        }
        glUniform1i(glGetUniformLocation(program, "uTexture0"), 0);
        glUniform1i(glGetUniformLocation(program, "uTexture1"), 1);
        glUniform1i(glGetUniformLocation(program, "uTexture2"), 2);
        glUniform1i(glGetUniformLocation(program, "uTexture3"), 3);
        glUniform1f(glGetUniformLocation(program, "uTime"), time);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        eglSwapBuffers(display_, surface_);

        timer.endFrame();
        time += 0.016f;
    }

    glDeleteTextures(4, textures);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(program);

    TestResult result;
    result.name = "Texture Sampling";
    result.api = getApiName();
    result.fps = timer.getAverageFps();
    result.frameTimeMs = timer.getAverageMs();
    result.triangles = 2;
    result.drawCalls = 1;
    return result;
}

// 公共几何生成方法
void GLEngine::generateSphere(int slices, int stacks, std::vector<float>& vertices, std::vector<uint32_t>& indices) {
    // 调用 MathUtils.h 中的实现
    ::generateSphere(slices, stacks, vertices, indices);
}

} // namespace GpuBench
