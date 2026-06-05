package com.gpubench.db;

import androidx.room.Dao;
import androidx.room.Delete;
import androidx.room.Insert;
import androidx.room.Query;

import java.util.List;

@Dao
public interface TestRecordDao {

    @Insert
    void insert(TestRecord record);

    @Delete
    void delete(TestRecord record);

    @Query("SELECT * FROM test_records ORDER BY timestamp DESC")
    List<TestRecord> getAllRecords();

    @Query("SELECT * FROM test_records ORDER BY timestamp DESC LIMIT :limit")
    List<TestRecord> getRecentRecords(int limit);

    @Query("SELECT * FROM test_records WHERE apiType = :apiType ORDER BY timestamp DESC")
    List<TestRecord> getRecordsByApi(String apiType);

    @Query("SELECT * FROM test_records ORDER BY avgFps DESC LIMIT 1")
    TestRecord getBestRecord();

    @Query("DELETE FROM test_records")
    void deleteAll();

    @Query("SELECT COUNT(*) FROM test_records")
    int getRecordCount();
}
