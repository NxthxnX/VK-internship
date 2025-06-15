#pragma once

#include <string>
#include <ostream>
#include <mutex>

// Абстрактный базовый класс для любых метрик
// write_value_to_stream(std::ostream& os) - запись значения в поток
// reset() - сброс накопленного значения
// mutex_ - мюьтекс для защиты данных при доступе из разных потоков 
class IMetric {
public:
    explicit IMetric(std::string name) : name_(std::move(name)) {}

    virtual void write_value_to_stream(std::ostream& os) = 0;

    virtual void reset() = 0;

    const std::string& get_name() const {
        return name_;
    }

protected:
    std::string name_;
    std::mutex mutex_;
};