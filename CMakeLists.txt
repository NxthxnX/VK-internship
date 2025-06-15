cmake_minimum_required(VERSION 3.10)
project(MetricsLibraryExample CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем флаг для работы с потоками
find_package(Threads REQUIRED)

add_executable(metrics_app main.cpp)

# Линкуем библиотеку потоков
target_link_libraries(metrics_app PRIVATE Threads::Threads)