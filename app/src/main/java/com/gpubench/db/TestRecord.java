package com.gpubench.db;

import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "test_records")
public class TestRecord {
    @PrimaryKey(autoGenerate = true)
    public int id;

    public long timestamp;        // 测试时间戳
    public String deviceModel;    // 设备型号
    public String androidVersion; // Android 版本
    public String apiType;        // GL 或 Vulkan
    public float avgFps;          // 平均 FPS
    public float avgFrameTime;    // 平均帧时间
    public long triangleCount;    // 三角形数量
    public int testDuration;      // 测试时长（秒）
    public String gpuRenderer;    // GPU 渲染器名称

    public TestRecord() {
        this.timestamp = System.currentTimeMillis();
    }

    @Override
    public String toString() {
        return String.format("%s | %s | FPS: %.0f", deviceModel, apiType, avgFps);
    }
}
