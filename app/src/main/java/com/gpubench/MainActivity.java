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
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import org.json.JSONObject;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "GpuBench";

    static {
        System.loadLibrary("gpubench");
    }

    // JNI 方法
    private native boolean nativeInitGL(Surface surface);
    private native boolean nativeInitVulkan(Surface surface);
    private native String nativeRunGLTest(String testName, int frameCount);
    private native String nativeRunVulkanTest(String testName, int frameCount);
    private native String[] nativeGetGLTestNames();
    private native String[] nativeGetVulkanTestNames();
    private native void nativeShutdown();
    private native String nativeGetDeviceInfo();

    // UI 组件
    private SurfaceView surfaceView;
    private TextView tvStatus;
    private TextView tvResults;
    private TextView tvSettings;
    private Button btnRunGL;
    private Button btnRunVulkan;
    private Button btnRunBoth;
    private Button btnSettings;
    private ProgressBar progressBar;
    private ScrollView scrollView;

    private Handler handler;
    private boolean isRunning = false;

    // 设置参数
    private String selectedApi = "GL";
    private int frameCount = 300;
    private boolean vsync = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        handler = new Handler(Looper.getMainLooper());

        // 从 Intent 获取设置参数
        Intent intent = getIntent();
        if (intent.hasExtra(SettingsActivity.KEY_API)) {
            selectedApi = intent.getStringExtra(SettingsActivity.KEY_API);
            frameCount = intent.getIntExtra(SettingsActivity.KEY_FRAME_COUNT, 300);
            vsync = intent.getBooleanExtra(SettingsActivity.KEY_VSYNC, false);
        }

        // 初始化 UI
        surfaceView = findViewById(R.id.surfaceView);
        tvStatus = findViewById(R.id.tvStatus);
        tvResults = findViewById(R.id.tvResults);
        tvSettings = findViewById(R.id.tvSettings);
        btnRunGL = findViewById(R.id.btnRunGL);
        btnRunVulkan = findViewById(R.id.btnRunVulkan);
        btnRunBoth = findViewById(R.id.btnRunBoth);
        btnSettings = findViewById(R.id.btnSettings);
        progressBar = findViewById(R.id.progressBar);
        scrollView = findViewById(R.id.scrollView);

        surfaceView.getHolder().addCallback(this);

        // 按钮事件
        btnRunGL.setOnClickListener(v -> runTests("GL"));
        btnRunVulkan.setOnClickListener(v -> runTests("Vulkan"));
        btnRunBoth.setOnClickListener(v -> runTests("Both"));
        btnSettings.setOnClickListener(v -> {
            Intent settingsIntent = new Intent(MainActivity.this, SettingsActivity.class);
            startActivity(settingsIntent);
            finish();
        });

        // 显示当前设置
        updateSettingsInfo();

        // 初始状态
        updateStatus("等待 Surface 创建...");
        setButtonsEnabled(false);
    }

    private void updateSettingsInfo() {
        String apiName;
        switch (selectedApi) {
            case "GL":
                apiName = "OpenGL ES 3.2";
                break;
            case "Vulkan":
                apiName = "Vulkan 1.3";
                break;
            default:
                apiName = "全部测试";
                break;
        }
        tvSettings.setText(String.format("API: %s | 帧数: %d | VSync: %s",
                apiName, frameCount, vsync ? "开" : "关"));
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        updateStatus("Surface 已创建，初始化引擎...");
        new Thread(() -> {
            // 根据选择的 API 初始化引擎
            boolean initOk = false;
            String apiName = "OpenGL ES";

            if (selectedApi.equals("Vulkan")) {
                initOk = nativeInitVulkan(holder.getSurface());
                if (initOk) {
                    apiName = "Vulkan";
                } else {
                    // Vulkan 失败，尝试 GL
                    initOk = nativeInitGL(holder.getSurface());
                    if (initOk) {
                        selectedApi = "GL";
                        apiName = "OpenGL ES";
                    }
                }
            } else {
                initOk = nativeInitGL(holder.getSurface());
            }

            final boolean success = initOk;
            final String finalApiName = apiName;
            handler.post(() -> {
                if (success) {
                    updateStatus(finalApiName + " 引擎初始化成功");
                    setButtonsEnabled(true);
                } else {
                    updateStatus("引擎初始化失败");
                }
            });
        }).start();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "Surface changed: " + width + "x" + height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeShutdown();
        setButtonsEnabled(false);
        updateStatus("Surface 已销毁");
    }

    private void runTests(String api) {
        if (isRunning) return;

        isRunning = true;
        setButtonsEnabled(false);
        progressBar.setVisibility(View.VISIBLE);
        tvResults.setText("");

        new Thread(() -> {
            try {
                if (api.equals("GL") || api.equals("Both")) {
                    // 确保 GL 引擎已初始化
                    if (selectedApi.equals("Vulkan")) {
                        // 需要重新初始化为 GL
                        nativeShutdown();
                        boolean glOk = nativeInitGL(surfaceView.getHolder().getSurface());
                        if (!glOk) {
                            appendResult("❌ OpenGL ES 初始化失败\n");
                            return;
                        }
                    }
                    runGLTests();
                }

                if (api.equals("Vulkan") || api.equals("Both")) {
                    // 需要重新初始化 Vulkan
                    nativeShutdown();
                    handler.post(() -> updateStatus("初始化 Vulkan 引擎..."));

                    boolean vkOk = nativeInitVulkan(surfaceView.getHolder().getSurface());
                    if (vkOk) {
                        runVulkanTests();
                    } else {
                        appendResult("❌ Vulkan 初始化失败（设备可能不支持）\n");
                    }
                }

                handler.post(() -> {
                    updateStatus("测试完成！");
                    appendResult("\n========== 测试完成 ==========\n");
                });

            } catch (Exception e) {
                Log.e(TAG, "Test error", e);
                handler.post(() -> updateStatus("测试出错: " + e.getMessage()));
            } finally {
                isRunning = false;
                handler.post(() -> {
                    setButtonsEnabled(true);
                    progressBar.setVisibility(View.GONE);
                });
            }
        }).start();
    }

    private void runGLTests() {
        String[] testNames = nativeGetGLTestNames();
        if (testNames == null) {
            appendResult("❌ 无法获取 GL 测试列表\n");
            return;
        }

        handler.post(() -> updateStatus("运行 OpenGL ES 测试..."));

        for (String testName : testNames) {
            handler.post(() -> updateStatus("GL: " + testName + "..."));

            String resultJson = nativeRunGLTest(testName, frameCount);
            try {
                JSONObject json = new JSONObject(resultJson);
                String result = String.format(
                    "🔵 [GL] %s\n" +
                    "   FPS: %.1f\n" +
                    "   帧时间: %.2f ms\n" +
                    "   三角形: %s\n" +
                    "   Draw Calls: %s\n\n",
                    json.getString("name"),
                    json.getDouble("fps"),
                    json.getDouble("frameTime"),
                    formatNumber(json.getLong("triangles")),
                    formatNumber(json.getLong("drawCalls"))
                );
                appendResult(result);
            } catch (Exception e) {
                appendResult("❌ GL 测试失败: " + testName + "\n");
            }
        }
    }

    private void runVulkanTests() {
        String[] testNames = nativeGetVulkanTestNames();
        if (testNames == null) {
            appendResult("❌ 无法获取 Vulkan 测试列表\n");
            return;
        }

        handler.post(() -> updateStatus("运行 Vulkan 测试..."));

        for (String testName : testNames) {
            handler.post(() -> updateStatus("Vulkan: " + testName + "..."));

            String resultJson = nativeRunVulkanTest(testName, frameCount);
            try {
                JSONObject json = new JSONObject(resultJson);
                String result = String.format(
                    "🟠 [Vulkan] %s\n" +
                    "   FPS: %.1f\n" +
                    "   帧时间: %.2f ms\n" +
                    "   三角形: %s\n" +
                    "   Draw Calls: %s\n\n",
                    json.getString("name"),
                    json.getDouble("fps"),
                    json.getDouble("frameTime"),
                    formatNumber(json.getLong("triangles")),
                    formatNumber(json.getLong("drawCalls"))
                );
                appendResult(result);
            } catch (Exception e) {
                appendResult("❌ Vulkan 测试失败: " + testName + "\n");
            }
        }
    }

    private void updateStatus(String status) {
        tvStatus.setText("状态: " + status);
    }

    private void appendResult(String text) {
        handler.post(() -> {
            tvResults.append(text);
            scrollView.post(() -> scrollView.fullScroll(View.FOCUS_DOWN));
        });
    }

    private void setButtonsEnabled(boolean enabled) {
        btnRunGL.setEnabled(enabled);
        btnRunVulkan.setEnabled(enabled);
        btnRunBoth.setEnabled(enabled);
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
    protected void onDestroy() {
        super.onDestroy();
        nativeShutdown();
    }
}
