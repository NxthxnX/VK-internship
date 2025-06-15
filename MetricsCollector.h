#pragma once

#include "IMetric.h"
#include "MetricTypes.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <stdexcept>
#include <utility>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iomanip>
#include <sstream>

// ���������� ��������� ����� � ������ ������� �YYYY-MM-DD HH:MM:SS.ms�
std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(tp);

    // ������������� ������ std::tm tm = *std::localtime(&t);, �. �. localtime(...) �� �������� ����������
    struct tm tm;
    if (localtime_s(&tm, &t) != 0) {
        throw std::runtime_error("������ ����������� time_t � localtime");
    }

    std::stringstream ss;
    ss << std::put_time(&tm, "%F %T");
    ss << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

class MetricsCollector {
public:
    MetricsCollector(const std::string& filename, std::chrono::milliseconds dump_interval)
        : output_filename_(filename), dump_interval_(dump_interval), is_running_(false) {
    }

    ~MetricsCollector() {
        stop();
    }

    // ������ �� ����������� � ������������: ����� ��������� �������
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    // ������ �������� ������ ��� ������ ������
    void start() {
        if (is_running_) {
            return;
        }

        output_file_.open(output_filename_, std::ios::app);
        if (!output_file_.is_open()) {
            throw std::runtime_error("Cannot open metrics file: " + output_filename_);
        }

        is_running_ = true;
        writer_thread_ = std::thread(&MetricsCollector::writer_loop, this);
    }

    // ��������� �������� ������
    void stop() {
        if (!is_running_) {
            return;
        }

        is_running_ = false;
        stop_cv_.notify_one(); // ����������� ������, ��� ��� ����������
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }

        if (output_file_.is_open()) {
            output_file_.close();
        }
    }

    // ��������� ����� ��� ����������� ����� ������� ������ ����
    template<typename MetricType, typename... Args>
    std::shared_ptr<MetricType> register_metric(const std::string& name, Args&&... args) {
        std::lock_guard<std::mutex> lock(map_mutex_);
        if (metrics_map_.count(name)) {
            // ������������ ������� ���������� - ���������� ������������ �������
            throw std::runtime_error("Metric with name '" + name + "' already exists.");
        }
        auto metric = std::make_shared<MetricType>(name, std::forward<Args>(args)...);
        metrics_map_[name] = metric;
        return metric;
    }

    // ��������� ��������� �� ��� ������������������ �������
    template<typename MetricType>
    std::shared_ptr<MetricType> get_metric(const std::string& name) {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto it = metrics_map_.find(name);
        if (it == metrics_map_.end()) {
            return nullptr; // ������� �� �������
        }
        // dynamic_pointer_cast ��� ����������� ���������� �����
        return std::dynamic_pointer_cast<MetricType>(it->second);
    }

private:
    // �������� ��� ������ ������
    void writer_loop() {
        while (is_running_) {
            // �������� ��������� ������� ��� ������� �� ���������
            std::unique_lock<std::mutex> lock(stop_mutex_);
            if (stop_cv_.wait_for(lock, dump_interval_, [this] { return !is_running_.load(); })) {
                // ����� �� ��������, ����� is_running_ ���������� false/����� dump_interval_
                break;
            }

            dump_metrics();
        }
    }

    // ������ ������
    void dump_metrics() {
        std::string timestamp = format_timestamp(std::chrono::system_clock::now());
        std::stringstream line_stream;
        line_stream << timestamp;

        std::vector<std::shared_ptr<IMetric>> metrics_to_dump;
        {
            // ���������� ���� �� ����� ����������� ����������
            std::lock_guard<std::mutex> lock(map_mutex_);
            for (const auto& pair : metrics_map_) {
                metrics_to_dump.push_back(pair.second);
            }
        }

        // ������ � ����� ���������� ��� ���������� ����
        for (const auto& metric : metrics_to_dump) {
            line_stream << " \"" << metric->get_name() << "\" ";
            metric->write_value_to_stream(line_stream);
        }

        output_file_ << line_stream.str() << std::endl;

        // ����� ������ ����� ������
        for (const auto& metric : metrics_to_dump) {
            metric->reset();
        }
    }

    std::string output_filename_;
    std::chrono::milliseconds dump_interval_;
    std::ofstream output_file_;

    std::map<std::string, std::shared_ptr<IMetric>> metrics_map_;
    std::mutex map_mutex_; // ������ ������� � metrics_map_

    std::thread writer_thread_;
    std::atomic<bool> is_running_;
    std::condition_variable stop_cv_;
    std::mutex stop_mutex_; // ��� ������������� � stop_cv_
};