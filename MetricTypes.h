#pragma once

#include "IMetric.h"
#include <atomic>
#include <iomanip>

// Метрика-счетчик для подсчитывания количества событий
// Может использоваться для подсчёта количества HTTP-запросов
// std::atomic - потокобезопасные инкременты
class CounterMetric : public IMetric {
public:
    explicit CounterMetric(const std::string& name) : IMetric(name) {}

    void increment(int value = 1) {
        // fetch_add потокобезопасен; relaxed режим подходит для счётчика
        counter_.fetch_add(value, std::memory_order_relaxed);
    }

    void write_value_to_stream(std::ostream& os) override {
        // memory_order_relaxed достаточно, так как нам не нужно синхронизировать другие данные
        os << counter_.load(std::memory_order_relaxed);
    }

    void reset() override {
        counter_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<int> counter_{ 0 };
};

// Метрика для вычисления среднего значения
// Может использоваться для средней утилизации CPU за секунду
class AverageMetric : public IMetric {
public:
    explicit AverageMetric(const std::string& name) : IMetric(name) {}

    void add_sample(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ += value;
        ++count_;
    }

    void write_value_to_stream(std::ostream& os) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) {
            os << 0.0;
        }
        else {
            os << std::setprecision(2) << std::fixed << (sum_ / count_);
        }
    }

    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ = 0.0;
        count_ = 0;
    }

private:
    double sum_{ 0.0 };
    int count_{ 0 };
};
