package com.gpubench;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;

import com.gpubench.db.AppDatabase;
import com.gpubench.db.TestRecord;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "GpuBench";

    static {
        System.loadLibrary("gpubench");
    }

    // JNI 方法
    private native boolean nativeInitGL(Surface surface);
    private native boolean nativeInitVulkan(Surface surface);
    private native void nativeShutdown();
    private native boolean nativeIsRunning();
    private native float nativeGetCurrentFps();
    private native float nativeGetCurrentFrameTime();
    private native long nativeGetTriangleCount();
    private native String nativeGetDeviceInfo();
    private native float[] nativeRunComputeTest();

    // UI 组件
    private SurfaceView surfaceView;
    private TextView tvTitle;
    private TextView tvDeviceInfo;
    private TextView tvFps;
    private TextView tvInfo;
    private TextView tvComputeResult;
    private TextView tvStatus;
    private Button btnGL;
    private Button btnVulkan;
    private Button btnStart;
    private Button btnCompute;
    private Button btnHistory;

    // 数据库
    private AppDatabase db;

    // FPS 统计
    private float totalFps = 0;
    private int fpsCount = 0;
    private long testStartTime = 0;

    private Handler handler;
    private boolean isRunning = false;
    private boolean surfaceReady = false;
    private String selectedApi = "GL";

    // FPS 更新定时器
    private Runnable fpsUpdater;
    private static final int FPS_UPDATE_INTERVAL = 100;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            // 全屏显示
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN
            );

            setContentView(R.layout.activity_main);

            handler = new Handler(Looper.getMainLooper());

            // 初始化数据库
            db = AppDatabase.getInstance(this);

            // 初始化 UI
            surfaceView = findViewById(R.id.surfaceView);
            tvTitle = findViewById(R.id.tvTitle);
            tvDeviceInfo = findViewById(R.id.tvDeviceInfo);
            tvFps = findViewById(R.id.tvFps);
            tvInfo = findViewById(R.id.tvInfo);
            tvComputeResult = findViewById(R.id.tvComputeResult);
            tvStatus = findViewById(R.id.tvStatus);
            btnGL = findViewById(R.id.btnGL);
            btnVulkan = findViewById(R.id.btnVulkan);
            btnStart = findViewById(R.id.btnStart);
            btnCompute = findViewById(R.id.btnCompute);
            btnHistory = findViewById(R.id.btnHistory);

            surfaceView.getHolder().addCallback(this);

            // API 选择按钮
            btnGL.setOnClickListener(v -> {
                selectedApi = "GL";
                btnGL.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF2196F3));
                btnVulkan.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF666666));
                updateStatus("已选择 OpenGL ES");
            });

            btnVulkan.setOnClickListener(v -> {
                selectedApi = "Vulkan";
                btnVulkan.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFFFF9800));
                btnGL.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF666666));
                updateStatus("已选择 Vulkan");
            });

            // 开始/停止按钮
            btnStart.setOnClickListener(v -> {
                if (isRunning) {
                    stopTest();
                } else {
                    startTest();
                }
            });

            // 计算测试按钮
            btnCompute.setOnClickListener(v -> {
                if (!isRunning) {
                    runComputeTest();
                }
            });

            // 历史记录按钮
            btnHistory.setOnClickListener(v -> {
                Intent intent = new Intent(MainActivity.this, HistoryActivity.class);
                startActivity(intent);
            });

            // 默认选中 GL
            btnGL.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF2196F3));
            btnVulkan.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF666666));

            // 显示设备信息
            loadDeviceInfo();

            // FPS 更新任务
            fpsUpdater = new Runnable() {
                @Override
                public void run() {
                    if (isRunning) {
                        updateFpsDisplay();
                        handler.postDelayed(this, FPS_UPDATE_INTERVAL);
                    }
                }
            };

            Log.d(TAG, "MainActivity created successfully");
        } catch (Exception e) {
            Log.e(TAG, "Error in onCreate", e);
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "Surface created");
        surfaceReady = true;
        updateStatus("就绪 - 选择 API 并开始测试");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "Surface changed: " + width + "x" + height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "Surface destroyed");
        surfaceReady = false;
        if (isRunning) {
            stopTest();
        }
        nativeShutdown();
    }

    private void startTest() {
        if (!surfaceReady) {
            updateStatus("等待 Surface 就绪...");
            return;
        }

        isRunning = true;
        totalFps = 0;
        fpsCount = 0;
        testStartTime = System.currentTimeMillis();

        btnStart.setText("⏹ 停止测试");
        btnStart.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFFF44336));
        btnGL.setEnabled(false);
        btnVulkan.setEnabled(false);
        btnHistory.setEnabled(false);

        tvTitle.setText("🎮 GPU Benchmark - " + (selectedApi.equals("GL") ? "OpenGL ES" : "Vulkan"));

        new Thread(() -> {
            try {
                // 初始化引擎
                boolean initOk;
                if (selectedApi.equals("Vulkan")) {
                    initOk = nativeInitVulkan(surfaceView.getHolder().getSurface());
                    if (!initOk) {
                        handler.post(() -> updateStatus("Vulkan 初始化失败，尝试 OpenGL ES..."));
                        initOk = nativeInitGL(surfaceView.getHolder().getSurface());
                    }
                } else {
                    initOk = nativeInitGL(surfaceView.getHolder().getSurface());
                }

                if (!initOk) {
                    handler.post(() -> {
                        updateStatus("初始化失败");
                        stopTest();
                    });
                    return;
                }

                handler.post(() -> updateStatus("正在运行测试..."));

                // 开始 FPS 更新
                handler.post(fpsUpdater);

            } catch (Exception e) {
                Log.e(TAG, "Error starting test", e);
                handler.post(() -> {
                    updateStatus("错误: " + e.getMessage());
                    stopTest();
                });
            }
        }).start();
    }

    private void stopTest() {
        isRunning = false;

        handler.removeCallbacks(fpsUpdater);

        // 计算平均 FPS 并保存记录
        if (fpsCount > 0) {
            float avgFps = totalFps / fpsCount;
            long testDuration = (System.currentTimeMillis() - testStartTime) / 1000;
            saveTestRecord(avgFps, testDuration);
        }

        btnStart.setText("▶ 开始测试");
        btnStart.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF4CAF50));
        btnGL.setEnabled(true);
        btnVulkan.setEnabled(true);
        btnHistory.setEnabled(true);

        updateStatus("测试已停止");
    }

    private void updateFpsDisplay() {
        try {
            float fps = nativeGetCurrentFps();
            float frameTime = nativeGetCurrentFrameTime();
            long triangles = nativeGetTriangleCount();

            // 累积 FPS 统计
            if (fps > 0) {
                totalFps += fps;
                fpsCount++;
            }

            // 更新 FPS 显示
            String fpsText = String.format("%.0f", fps);
            tvFps.setText("FPS: " + fpsText);

            // 根据 FPS 设置颜色
            if (fps >= 55) {
                tvFps.setTextColor(0xFF00FF00); // 绿色 - 流畅
            } else if (fps >= 30) {
                tvFps.setTextColor(0xFFFFFF00); // 黄色 - 一般
            } else {
                tvFps.setTextColor(0xFFFF0000); // 红色 - 卡顿
            }

            // 更新详细信息
            String info = String.format("帧时间: %.1f ms | 三角形: %s", frameTime, formatNumber(triangles));
            tvInfo.setText(info);
        } catch (Exception e) {
            Log.e(TAG, "Error updating FPS", e);
        }
    }

    private void updateStatus(String status) {
        if (tvStatus != null) {
            tvStatus.setText(status);
        }
    }

    private void loadDeviceInfo() {
        try {
            // 获取 Android 设备信息
            String manufacturer = android.os.Build.MANUFACTURER;
            String model = android.os.Build.MODEL;
            String device = android.os.Build.DEVICE;
            int sdkVersion = android.os.Build.VERSION.SDK_INT;
            String androidVersion = android.os.Build.VERSION.RELEASE;

            // 获取 OpenGL ES 信息
            android.opengl.GLES20.glGetString(android.opengl.GLES20.GL_RENDERER);
            android.opengl.GLES20.glGetString(android.opengl.GLES20.GL_VERSION);
            android.opengl.GLES20.glGetString(android.opengl.GLES20.GL_VENDOR);

            // 构建设备信息字符串
            StringBuilder info = new StringBuilder();
            info.append(String.format("%s %s | Android %s", manufacturer, model, androidVersion));

            tvDeviceInfo.setText(info.toString());
            tvDeviceInfo.setTextSize(11);

            Log.d(TAG, "Device: " + manufacturer + " " + model);
            Log.d(TAG, "Android: " + androidVersion + " (API " + sdkVersion + ")");

        } catch (Exception e) {
            Log.e(TAG, "Error loading device info", e);
            tvDeviceInfo.setText("设备信息加载失败");
        }
    }

    private String formatNumber(long number) {
        if (number >= 1_000_000) {
            return String.format("%.1fM", number / 1_000_000.0);
        } else if (number >= 1_000) {
            return String.format("%.1fK", number / 1_000.0);
        }
        return String.valueOf(number);
    }

    private void saveTestRecord(float avgFps, long testDuration) {
        new Thread(() -> {
            try {
                TestRecord record = new TestRecord();
                record.timestamp = System.currentTimeMillis();
                record.deviceModel = android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL;
                record.androidVersion = android.os.Build.VERSION.RELEASE;
                record.apiType = selectedApi;
                record.avgFps = avgFps;
                record.avgFrameTime = avgFps > 0 ? 1000.0f / avgFps : 0;
                record.triangleCount = nativeGetTriangleCount();
                record.testDuration = (int) testDuration;
                record.gpuRenderer = "GPU";

                db.testRecordDao().insert(record);

                Log.d(TAG, "Test record saved: " + avgFps + " FPS");
            } catch (Exception e) {
                Log.e(TAG, "Error saving test record", e);
            }
        }).start();
    }

    private void runComputeTest() {
        if (!surfaceReady) {
            updateStatus("等待 Surface 就绪...");
            return;
        }

        // 先初始化 GL 引擎
        new Thread(() -> {
            try {
                handler.post(() -> {
                    updateStatus("正在初始化引擎...");
                    btnCompute.setEnabled(false);
                    btnStart.setEnabled(false);
                });

                boolean initOk = nativeInitGL(surfaceView.getHolder().getSurface());
                if (!initOk) {
                    handler.post(() -> {
                        updateStatus("引擎初始化失败");
                        btnCompute.setEnabled(true);
                        btnStart.setEnabled(true);
                    });
                    return;
                }

                handler.post(() -> updateStatus("正在运行计算测试..."));

                // 运行计算测试
                float[] result = nativeRunComputeTest();

                if (result != null && result.length >= 3) {
                    float gflops = result[0];
                    float bandwidth = result[1];
                    float score = result[2];

                    // 显示结果
                    String resultText = String.format("GFLOPS: %.1f | 带宽: %.1f GB/s | 分数: %.0f",
                            gflops, bandwidth, score);

                    handler.post(() -> {
                        tvComputeResult.setText(resultText);
                        updateStatus("计算测试完成");
                        btnCompute.setEnabled(true);
                        btnStart.setEnabled(true);
                    });

                    // 保存记录
                    saveComputeTestRecord(gflops, bandwidth, score);

                    Log.d(TAG, "Compute test result: " + resultText);
                } else {
                    handler.post(() -> {
                        updateStatus("计算测试失败");
                        btnCompute.setEnabled(true);
                        btnStart.setEnabled(true);
                    });
                }

                // 关闭引擎
                nativeShutdown();

            } catch (Exception e) {
                Log.e(TAG, "Error running compute test", e);
                handler.post(() -> {
                    updateStatus("计算测试错误: " + e.getMessage());
                    btnCompute.setEnabled(true);
                    btnStart.setEnabled(true);
                });
            }
        }).start();
    }

    private void saveComputeTestRecord(float gflops, float bandwidth, float score) {
        new Thread(() -> {
            try {
                TestRecord record = new TestRecord();
                record.timestamp = System.currentTimeMillis();
                record.deviceModel = android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL;
                record.androidVersion = android.os.Build.VERSION.RELEASE;
                record.apiType = "Compute";
                record.avgFps = gflops; // 用 GFLOPS 作为主要指标
                record.avgFrameTime = bandwidth;
                record.triangleCount = (long) score;
                record.testDuration = 0;
                record.gpuRenderer = String.format("%.1f GFLOPS", gflops);

                db.testRecordDao().insert(record);

                Log.d(TAG, "Compute test record saved: " + gflops + " GFLOPS");
            } catch (Exception e) {
                Log.e(TAG, "Error saving compute test record", e);
            }
        }).start();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 隐藏系统 UI
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isRunning) {
            stopTest();
        }
        nativeShutdown();
    }
}
