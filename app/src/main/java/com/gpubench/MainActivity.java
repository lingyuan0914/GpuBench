package com.gpubench;

import android.app.Activity;
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

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "GpuBench";

    static {
        System.loadLibrary("gpubench");
    }

    // JNI 方法
    private native boolean nativeInitGL(Surface surface);
    private native boolean nativeInitVulkan(Surface surface);
    private native boolean nativeRunBenchmark(String api, int durationFrames);
    private native void nativeStopBenchmark();
    private native void nativeShutdown();
    private native boolean nativeIsRunning();
    private native float nativeGetCurrentFps();
    private native float nativeGetCurrentFrameTime();
    private native long nativeGetTriangleCount();

    // UI 组件
    private SurfaceView surfaceView;
    private TextView tvTitle;
    private TextView tvFps;
    private TextView tvInfo;
    private TextView tvStatus;
    private Button btnGL;
    private Button btnVulkan;
    private Button btnStart;

    private Handler handler;
    private boolean isRunning = false;
    private boolean surfaceReady = false;
    private String selectedApi = "GL"; // GL 或 Vulkan

    // FPS 更新定时器
    private Runnable fpsUpdater;
    private static final int FPS_UPDATE_INTERVAL = 100; // 每100ms更新一次

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 全屏显示
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );

        setContentView(R.layout.activity_main);

        handler = new Handler(Looper.getMainLooper());

        // 初始化 UI
        surfaceView = findViewById(R.id.surfaceView);
        tvTitle = findViewById(R.id.tvTitle);
        tvFps = findViewById(R.id.tvFps);
        tvInfo = findViewById(R.id.tvInfo);
        tvStatus = findViewById(R.id.tvStatus);
        btnGL = findViewById(R.id.btnGL);
        btnVulkan = findViewById(R.id.btnVulkan);
        btnStart = findViewById(R.id.btnStart);

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

        // 默认选中 GL
        btnGL.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF2196F3));
        btnVulkan.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF666666));

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
        btnStart.setText("⏹ 停止测试");
        btnStart.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFFF44336));
        btnGL.setEnabled(false);
        btnVulkan.setEnabled(false);

        tvTitle.setText("🎮 GPU Benchmark - " + (selectedApi.equals("GL") ? "OpenGL ES" : "Vulkan"));

        new Thread(() -> {
            // 初始化引擎
            boolean initOk;
            if (selectedApi.equals("Vulkan")) {
                initOk = nativeInitVulkan(surfaceView.getHolder().getSurface());
                if (!initOk) {
                    handler.post(() -> {
                        updateStatus("Vulkan 初始化失败，尝试 OpenGL ES...");
                        // 回退到 GL
                        new Thread(() -> {
                            boolean glOk = nativeInitGL(surfaceView.getHolder().getSurface());
                            if (glOk) {
                                handler.post(() -> updateStatus("使用 OpenGL ES 运行"));
                                nativeRunBenchmark("GL", 0); // 0 = 持续运行
                            } else {
                                handler.post(() -> {
                                    updateStatus("初始化失败");
                                    stopTest();
                                });
                            }
                        }).start();
                    });
                    return;
                }
            } else {
                initOk = nativeInitGL(surfaceView.getHolder().getSurface());
                if (!initOk) {
                    handler.post(() -> {
                        updateStatus("OpenGL ES 初始化失败");
                        stopTest();
                    });
                    return;
                }
            }

            handler.post(() -> updateStatus("正在运行测试..."));

            // 开始 FPS 更新
            handler.post(fpsUpdater);

            // 运行基准测试 (持续运行直到调用 stop)
            nativeRunBenchmark(selectedApi, 0);

        }).start();
    }

    private void stopTest() {
        isRunning = false;
        nativeStopBenchmark();

        handler.removeCallbacks(fpsUpdater);

        btnStart.setText("▶ 开始测试");
        btnStart.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF4CAF50));
        btnGL.setEnabled(true);
        btnVulkan.setEnabled(true);

        updateStatus("测试已停止");
    }

    private void updateFpsDisplay() {
        float fps = nativeGetCurrentFps();
        float frameTime = nativeGetCurrentFrameTime();
        long triangles = nativeGetTriangleCount();

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
    }

    private void updateStatus(String status) {
        tvStatus.setText(status);
    }

    private String formatNumber(long number) {
        if (number >= 1_000_000) {
            return String.format("%.1fM", number / 1_000_000.0);
        } else if (number >= 1_000) {
            return String.format("%.1fK", number / 1_000.0);
        }
        return String.valueOf(number);
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
