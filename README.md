# VK internship
Solving the VK internship test task

# Структура проекта
IMetric.h - хедер абстрактного базового класса для любых типов метрик  
MetricTypes.h - реализация конкретных типов метрик (счётчик + mean value)  
MetricsCollector.h - основной класс, реализующий работу с потоками и файлами и выполняющий запись метрик  
main.cpp - пример использования данной библиотеки. Реализуется ежесекундная запись метрик в metrics.log  
CMakeLists.txt - файл для сборки проекта  
metrics.log - вывод работы программы  
