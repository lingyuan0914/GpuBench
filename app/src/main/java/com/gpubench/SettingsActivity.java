package com.gpubench;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.Button;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

public class SettingsActivity extends Activity {

    public static final String PREF_NAME = "GpuBenchPrefs";
    public static final String KEY_API = "api";
    public static final String KEY_FRAME_COUNT = "frame_count";
    public static final String KEY_VSYNC = "vsync";
    public static final String KEY_WARMUP = "warmup";

    private RadioGroup radioGroupApi;
    private RadioButton radioGL, radioVulkan, radioBoth;
    private SeekBar seekBarFrames;
    private TextView tvFrameCount;
    private Switch switchVsync;
    private Button btnStart;

    private SharedPreferences prefs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);

        prefs = getSharedPreferences(PREF_NAME, MODE_PRIVATE);

        // 初始化 UI
        radioGroupApi = findViewById(R.id.radioGroupApi);
        radioGL = findViewById(R.id.radioGL);
        radioVulkan = findViewById(R.id.radioVulkan);
        radioBoth = findViewById(R.id.radioBoth);
        seekBarFrames = findViewById(R.id.seekBarFrames);
        tvFrameCount = findViewById(R.id.tvFrameCount);
        switchVsync = findViewById(R.id.switchVsync);
        btnStart = findViewById(R.id.btnStart);

        // 加载保存的设置
        loadSettings();

        // SeekBar 事件
        seekBarFrames.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                // 最小 100，最大 1000，步长 100
                int frameCount = 100 + (progress * 100);
                tvFrameCount.setText(frameCount + " 帧");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // 开始按钮
        btnStart.setOnClickListener(v -> {
            saveSettings();
            Intent intent = new Intent(SettingsActivity.this, MainActivity.class);
            intent.putExtra(KEY_API, getSelectedApi());
            intent.putExtra(KEY_FRAME_COUNT, getFrameCount());
            intent.putExtra(KEY_VSYNC, switchVsync.isChecked());
            startActivity(intent);
        });
    }

    private void loadSettings() {
        String api = prefs.getString(KEY_API, "GL");
        int frameCount = prefs.getInt(KEY_FRAME_COUNT, 300);
        boolean vsync = prefs.getBoolean(KEY_VSYNC, false);

        // 设置 API 选择
        switch (api) {
            case "GL":
                radioGL.setChecked(true);
                break;
            case "Vulkan":
                radioVulkan.setChecked(true);
                break;
            case "Both":
                radioBoth.setChecked(true);
                break;
        }

        // 设置帧数
        int progress = (frameCount - 100) / 100;
        seekBarFrames.setProgress(progress);
        tvFrameCount.setText(frameCount + " 帧");

        // 设置垂直同步
        switchVsync.setChecked(vsync);
    }

    private void saveSettings() {
        SharedPreferences.Editor editor = prefs.edit();
        editor.putString(KEY_API, getSelectedApi());
        editor.putInt(KEY_FRAME_COUNT, getFrameCount());
        editor.putBoolean(KEY_VSYNC, switchVsync.isChecked());
        editor.apply();
    }

    private String getSelectedApi() {
        int id = radioGroupApi.getCheckedRadioButtonId();
        if (id == R.id.radioGL) return "GL";
        if (id == R.id.radioVulkan) return "Vulkan";
        return "Both";
    }

    private int getFrameCount() {
        return 100 + (seekBarFrames.getProgress() * 100);
    }
}
