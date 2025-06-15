#include "MetricsCollector.h"
#include <iostream>
#include <vector>
#include <random>

// Функция, имитирующая работу, которая генерирует метрики
void worker_thread_func(MetricsCollector& collector, int thread_id, std::atomic<bool>& keep_running) {
    std::cout << "Worker thread " << thread_id << " started.\n";

    // Получаем указатели на метрики один раз для эффективности
    auto http_counter = collector.get_metric<CounterMetric>("HTTP requests RPS");
    auto cpu_avg = collector.get_metric<AverageMetric>("CPU");

    if (!http_counter || !cpu_avg) {
        std::cerr << "Error: Could not get metrics in thread " << thread_id << std::endl;
        return;
    }

    std::random_device rd; // Истинно случайное зерно
    std::mt19937 gen(rd()); // Реализация алгоритма Mersenne Twister - псевдослучайного генератора
    std::uniform_int_distribution<> http_dist(1, 3); // Имитация количества HTTP-запросов
    std::uniform_real_distribution<> cpu_dist(0.4, 1.6); // Имитация загрузки части двух ядер

    while (keep_running) {
        http_counter->increment(http_dist(gen));

        cpu_avg->add_sample(cpu_dist(gen));

        // Имитация «полезной работы»
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "Worker thread " << thread_id << " finished.\n";
}


int main() {
    const std::string filename = "metrics.log";
    std::cout << "Starting metrics collection. Output will be saved to " << filename << std::endl;

    // Создание коллектор, который будет сбрасывать метрики в файл каждую секунду
    MetricsCollector collector(filename, std::chrono::seconds(1));

    try {
        collector.register_metric<CounterMetric>("HTTP requests RPS");
        collector.register_metric<AverageMetric>("CPU");

        // Запуск фонового потока записи
        collector.start();

        std::atomic<bool> keep_running = true;
        std::vector<std::thread> worker_threads;
        const int num_threads = 4;

        for (int i = 0; i < num_threads; ++i) {
            // Использование std::ref для передачи объекта по ссылке
            worker_threads.emplace_back(worker_thread_func, std::ref(collector), i + 1, std::ref(keep_running));
        }

        // Пусть система работает 10 секунд
        std::cout << "Running for 10 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));

        keep_running = false;
        for (std::thread& t : worker_threads) {
            t.join();
        }

        collector.stop();
    }
    catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        collector.stop();
        return -1;
    }

    std::cout << "Metrics collection finished. Check the '" << filename << "' file.\n";

    return 0;
}