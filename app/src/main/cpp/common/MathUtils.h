#pragma once

#include <cmath>
#include <array>
#include <cstring>
#include <vector>

namespace GpuBench {

// 向量结构
struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;
};

struct Vec4 {
    float x, y, z, w;
};

// 4x4 矩阵 (列主序)
struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 mat;
        memset(mat.m, 0, sizeof(mat.m));
        mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
        return mat;
    }

    static Mat4 perspective(float fovY, float aspect, float near, float far) {
        Mat4 mat;
        memset(mat.m, 0, sizeof(mat.m));
        float tanHalfFov = tanf(fovY / 2.0f);
        mat.m[0] = 1.0f / (aspect * tanHalfFov);
        mat.m[5] = 1.0f / tanHalfFov;
        mat.m[10] = -(far + near) / (far - near);
        mat.m[11] = -1.0f;
        mat.m[14] = -(2.0f * far * near) / (far - near);
        return mat;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = {center.x - eye.x, center.y - eye.y, center.z - eye.z};
        float len = sqrtf(f.x*f.x + f.y*f.y + f.z*f.z);
        f.x /= len; f.y /= len; f.z /= len;

        Vec3 s = {f.y*up.z - f.z*up.y, f.z*up.x - f.x*up.z, f.x*up.y - f.y*up.x};
        len = sqrtf(s.x*s.x + s.y*s.y + s.z*s.z);
        s.x /= len; s.y /= len; s.z /= len;

        Vec3 u = {s.y*f.z - s.z*f.y, s.z*f.x - s.x*f.z, s.x*f.y - s.y*f.x};

        Mat4 mat = identity();
        mat.m[0] = s.x; mat.m[4] = s.y; mat.m[8]  = s.z;
        mat.m[1] = u.x; mat.m[5] = u.y; mat.m[9]  = u.z;
        mat.m[2] = -f.x; mat.m[6] = -f.y; mat.m[10] = -f.z;
        mat.m[12] = -(s.x*eye.x + s.y*eye.y + s.z*eye.z);
        mat.m[13] = -(u.x*eye.x + u.y*eye.y + u.z*eye.z);
        mat.m[14] = (f.x*eye.x + f.y*eye.y + f.z*eye.z);
        return mat;
    }

    static Mat4 rotation(float angle, const Vec3& axis) {
        Mat4 mat = identity();
        float c = cosf(angle);
        float s = sinf(angle);
        float t = 1.0f - c;
        float x = axis.x, y = axis.y, z = axis.z;
        mat.m[0] = t*x*x + c;     mat.m[1] = t*x*y + s*z;   mat.m[2] = t*x*z - s*y;
        mat.m[4] = t*x*y - s*z;   mat.m[5] = t*y*y + c;     mat.m[6] = t*y*z + s*x;
        mat.m[8] = t*x*z + s*y;   mat.m[9] = t*y*z - s*x;   mat.m[10] = t*z*z + c;
        return mat;
    }

    static Mat4 translation(float x, float y, float z) {
        Mat4 mat = identity();
        mat.m[12] = x; mat.m[13] = y; mat.m[14] = z;
        return mat;
    }

    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                result.m[col * 4 + row] = 0;
                for (int k = 0; k < 4; k++) {
                    result.m[col * 4 + row] += m[k * 4 + row] * other.m[col * 4 + k];
                }
            }
        }
        return result;
    }
};

// 生成球体顶点数据
inline void generateSphere(int slices, int stacks,
                           std::vector<float>& vertices,
                           std::vector<uint32_t>& indices) {
    vertices.clear();
    indices.clear();

    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159265f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * 3.14159265f * j / slices;
            float x = sinf(phi) * cosf(theta);
            float y = cosf(phi);
            float z = sinf(phi) * sinf(theta);
            // position
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            // normal
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            // texcoord
            vertices.push_back((float)j / slices); vertices.push_back((float)i / stacks);
        }
    }

    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;
            indices.push_back(first); indices.push_back(second); indices.push_back(first + 1);
            indices.push_back(second); indices.push_back(second + 1); indices.push_back(first + 1);
        }
    }
}

// 生成平面网格
inline void generatePlane(int gridSize, std::vector<float>& vertices, std::vector<uint32_t>& indices) {
    vertices.clear();
    indices.clear();

    float step = 2.0f / gridSize;
    for (int z = 0; z <= gridSize; z++) {
        for (int x = 0; x <= gridSize; x++) {
            float px = -1.0f + x * step;
            float pz = -1.0f + z * step;
            // position
            vertices.push_back(px); vertices.push_back(0.0f); vertices.push_back(pz);
            // normal
            vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
            // texcoord
            vertices.push_back((float)x / gridSize); vertices.push_back((float)z / gridSize);
        }
    }

    for (int z = 0; z < gridSize; z++) {
        for (int x = 0; x < gridSize; x++) {
            int topLeft = z * (gridSize + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (gridSize + 1) + x;
            int bottomRight = bottomLeft + 1;
            indices.push_back(topLeft); indices.push_back(bottomLeft); indices.push_back(topRight);
            indices.push_back(topRight); indices.push_back(bottomLeft); indices.push_back(bottomRight);
        }
    }
}

} // namespace GpuBench
