package com.gpubench;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;

import com.gpubench.db.AppDatabase;
import com.gpubench.db.TestRecord;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class HistoryActivity extends Activity {
    private static final String TAG = "HistoryActivity";
    private static final SimpleDateFormat DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    private ListView listView;
    private TextView tvEmpty;
    private TextView tvBestScore;
    private TextView tvBestInfo;
    private Button btnBack;
    private Button btnClear;

    private AppDatabase db;
    private Handler handler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_history);

        handler = new Handler(Looper.getMainLooper());
        db = AppDatabase.getInstance(this);

        // 初始化 UI
        listView = findViewById(R.id.listView);
        tvEmpty = findViewById(R.id.tvEmpty);
        tvBestScore = findViewById(R.id.tvBestScore);
        tvBestInfo = findViewById(R.id.tvBestInfo);
        btnBack = findViewById(R.id.btnBack);
        btnClear = findViewById(R.id.btnClear);

        // 返回按钮
        btnBack.setOnClickListener(v -> finish());

        // 清空按钮
        btnClear.setOnClickListener(v -> {
            new Thread(() -> {
                db.testRecordDao().deleteAll();
                handler.post(this::loadData);
            }).start();
        });

        // 加载数据
        loadData();
    }

    private void loadData() {
        new Thread(() -> {
            try {
                // 获取最佳成绩
                TestRecord best = db.testRecordDao().getBestRecord();

                // 获取所有记录
                List<TestRecord> records = db.testRecordDao().getAllRecords();

                handler.post(() -> {
                    // 更新最佳成绩
                    if (best != null) {
                        tvBestScore.setText(String.format("FPS: %.0f", best.avgFps));
                        tvBestScore.setTextColor(getFpsColor(best.avgFps));
                        tvBestInfo.setText(String.format("%s | %s | %s",
                                best.deviceModel, best.apiType,
                                DATE_FORMAT.format(new Date(best.timestamp))));
                    } else {
                        tvBestScore.setText("FPS: --");
                        tvBestInfo.setText("暂无记录");
                    }

                    // 更新列表
                    if (records.isEmpty()) {
                        listView.setVisibility(View.GONE);
                        tvEmpty.setVisibility(View.VISIBLE);
                    } else {
                        listView.setVisibility(View.VISIBLE);
                        tvEmpty.setVisibility(View.GONE);
                        listView.setAdapter(new HistoryAdapter(records));
                    }
                });
            } catch (Exception e) {
                Log.e(TAG, "Error loading data", e);
            }
        }).start();
    }

    private int getFpsColor(float fps) {
        if (fps >= 55) return 0xFF00FF00; // 绿色
        if (fps >= 30) return 0xFFFFFF00; // 黄色
        return 0xFFFF0000; // 红色
    }

    // 列表适配器
    private class HistoryAdapter extends BaseAdapter {
        private final List<TestRecord> records;

        public HistoryAdapter(List<TestRecord> records) {
            this.records = records;
        }

        @Override
        public int getCount() {
            return records.size();
        }

        @Override
        public TestRecord getItem(int position) {
            return records.get(position);
        }

        @Override
        public long getItemId(int position) {
            return records.get(position).id;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = getLayoutInflater().inflate(R.layout.item_history, parent, false);
            }

            TestRecord record = getItem(position);

            // API 类型标识
            TextView tvApiType = convertView.findViewById(R.id.tvApiType);
            tvApiType.setText(record.apiType);
            if ("Vulkan".equals(record.apiType)) {
                tvApiType.setBackgroundColor(0xFFFF9800); // 橙色
            } else {
                tvApiType.setBackgroundColor(0xFF2196F3); // 蓝色
            }

            // 日期
            TextView tvDate = convertView.findViewById(R.id.tvDate);
            tvDate.setText(DATE_FORMAT.format(new Date(record.timestamp)));

            // 设备型号
            TextView tvDevice = convertView.findViewById(R.id.tvDevice);
            tvDevice.setText(record.deviceModel);

            // FPS
            TextView tvFps = convertView.findViewById(R.id.tvFps);
            tvFps.setText(String.format("%.0f", record.avgFps));
            tvFps.setTextColor(getFpsColor(record.avgFps));

            return convertView;
        }
    }
}
