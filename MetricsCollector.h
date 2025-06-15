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

// Приведение временной метки в строку формата «YYYY-MM-DD HH:MM:SS.ms»
std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(tp);

    // Использование вместо std::tm tm = *std::localtime(&t);, т. к. localtime(...) не является безопасной
    struct tm tm;
    if (localtime_s(&tm, &t) != 0) {
        throw std::runtime_error("Ошибка конвертации time_t в localtime");
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

    // Запрет на копирование и присваивание: проще управлять потоком
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    // Запуск фонового потока для записи метрик
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

    // Остановка фонового потока
    void stop() {
        if (!is_running_) {
            return;
        }

        is_running_ = false;
        stop_cv_.notify_one(); // Пробуждение потока, для его завершения
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }

        if (output_file_.is_open()) {
            output_file_.close();
        }
    }

    // Шаблонный метод для регистрации новой метрики любого типа
    template<typename MetricType, typename... Args>
    std::shared_ptr<MetricType> register_metric(const std::string& name, Args&&... args) {
        std::lock_guard<std::mutex> lock(map_mutex_);
        if (metrics_map_.count(name)) {
            // Альтернатива выбросу исключения - возвращать существующую метрику
            throw std::runtime_error("Metric with name '" + name + "' already exists.");
        }
        auto metric = std::make_shared<MetricType>(name, std::forward<Args>(args)...);
        metrics_map_[name] = metric;
        return metric;
    }

    // Получение указателя на уже зарегистрированную метрику
    template<typename MetricType>
    std::shared_ptr<MetricType> get_metric(const std::string& name) {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto it = metrics_map_.find(name);
        if (it == metrics_map_.end()) {
            return nullptr; // Метрика не найдена
        }
        // dynamic_pointer_cast для безопасного приведения типов
        return std::dynamic_pointer_cast<MetricType>(it->second);
    }

private:
    // Ожидание для записи метрик
    void writer_loop() {
        while (is_running_) {
            // Ожидание интервала времени или сигнала об остановке
            std::unique_lock<std::mutex> lock(stop_mutex_);
            if (stop_cv_.wait_for(lock, dump_interval_, [this] { return !is_running_.load(); })) {
                // Выход из ожидания, когда is_running_ становится false/после dump_interval_
                break;
            }

            dump_metrics();
        }
    }

    // Запись метрик
    void dump_metrics() {
        std::string timestamp = format_timestamp(std::chrono::system_clock::now());
        std::stringstream line_stream;
        line_stream << timestamp;

        std::vector<std::shared_ptr<IMetric>> metrics_to_dump;
        {
            // Блокировка мапы на время копирования указателей
            std::lock_guard<std::mutex> lock(map_mutex_);
            for (const auto& pair : metrics_map_) {
                metrics_to_dump.push_back(pair.second);
            }
        }

        // Запись и сброс происходят без блокировки мапы
        for (const auto& metric : metrics_to_dump) {
            line_stream << " \"" << metric->get_name() << "\" ";
            metric->write_value_to_stream(line_stream);
        }

        output_file_ << line_stream.str() << std::endl;

        // Ресет метрик после записи
        for (const auto& metric : metrics_to_dump) {
            metric->reset();
        }
    }

    std::string output_filename_;
    std::chrono::milliseconds dump_interval_;
    std::ofstream output_file_;

    std::map<std::string, std::shared_ptr<IMetric>> metrics_map_;
    std::mutex map_mutex_; // Защита доступа к metrics_map_

    std::thread writer_thread_;
    std::atomic<bool> is_running_;
    std::condition_variable stop_cv_;
    std::mutex stop_mutex_; // Для использования с stop_cv_
};
