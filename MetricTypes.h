#pragma once

#include "IMetric.h"
#include <atomic>
#include <iomanip>

// �������-������� ��� ������������� ���������� �������
// ����� �������������� ��� �������� ���������� HTTP-��������
// std::atomic - ���������������� ����������
class CounterMetric : public IMetric {
public:
    explicit CounterMetric(const std::string& name) : IMetric(name) {}

    void increment(int value = 1) {
        // fetch_add ���������������; relaxed ����� �������� ��� ��������
        counter_.fetch_add(value, std::memory_order_relaxed);
    }

    void write_value_to_stream(std::ostream& os) override {
        // memory_order_relaxed ����������, ��� ��� ��� �� ����� ���������������� ������ ������
        os << counter_.load(std::memory_order_relaxed);
    }

    void reset() override {
        counter_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<int> counter_{ 0 };
};

// ������� ��� ���������� �������� ��������
// ����� �������������� ��� ������� ���������� CPU �� �������
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