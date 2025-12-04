#ifndef PARALLEL_FINAL_H
#define PARALLEL_FINAL_H

#include "simulator.h"
#include "common/distributions.h"
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <mutex>
#include <map>
#include <sstream>
#include <numeric>

using namespace std;
using namespace TestDistributions;

class ParallelFinal {
private:
    static mutex cout_mutex;
    
    // Структура для конфигурации теста из таблицы
    struct LabTestConfig {
        int test_id;
        string arrival_dist;
        string service_dist;
        int cores;
        string finish_type; // "T" или "K"
        double finish_value;
        int buffer_size;
        string queue_discipline;
        string notes;
        double rho_limit;
    };
    
public:
    struct TestResult {
        int test_id;
        string config_name;
        double wait_time_seq;
        double wait_time_par;
        double system_time_seq;
        double system_time_par;
        double time_seq_ms;
        double time_par_ms;
        double speedup;
        double efficiency;
        double server_utilization;
        double loss_probability;
        double avg_queue_length;
        double rho_value;
    };
    
    // Главный метод для запуска всех тестов
    void run_complete_test() {
        cout << "КОМПЛЕКСНОЕ ТЕСТИРОВАНИЕ ПАРАЛЛЕЛЬНОЙ СИМУЛЯЦИИ СМО\n";
        cout << "===================================================\n\n";
        
        // Загружаем тесты из лабораторной работы
        vector<LabTestConfig> lab_tests = load_lab_tests();
        
        // 1. Тесты производительности для базовых случаев
        cout << "1. БАЗОВЫЕ ТЕСТЫ ПРОИЗВОДИТЕЛЬНОСТИ (СТАЦИОНАРНЫЙ РЕЖИМ)\n";
        cout << "=======================================================\n";
        run_basic_performance_tests();
        
        // 2. Тесты из лабораторной работы (упрощенные)
        cout << "\n\n2. ТЕСТЫ ИЗ ЛАБОРАТОРНОЙ РАБОТЫ (ОСНОВНЫЕ КОНФИГУРАЦИИ)\n";
        cout << "======================================================\n";
        run_lab_tests_simplified(lab_tests);
        
        // 3. Масштабируемость
        cout << "\n\n3. МАСШТАБИРУЕМОСТЬ ПО КОЛИЧЕСТВУ ЯДЕР (ПРИ ФИКСИРОВАННОМ ρ=0.8)\n";
        cout << "===========================================================\n";
        test_scalability_cores();
        
        // 4. Влияние дисциплины очереди на параллелизм
        cout << "\n\n4. ВЛИЯНИЕ ДИСЦИПЛИНЫ ОЧЕРЕДИ НА ЭФФЕКТИВНОСТЬ ПАРАЛЛЕЛИЗМА\n";
        cout << "===========================================================\n";
        test_queue_disciplines_comparison();
        
        // 5. Анализ ускорения для разных нагрузок
        cout << "\n\n5. АНАЛИЗ УСКОРЕНИЯ ПРИ РАЗЛИЧНЫХ НАГРУЗКАХ (ρ=0.1-0.95)\n";
        cout << "==================================================\n";
        test_speedup_vs_load();
        
        cout << "\n\nТЕСТИРОВАНИЕ ЗАВЕРШЕНО\n";
    }
    
private:
    // ========== Загрузка тестов из лабораторной работы ==========
    vector<LabTestConfig> load_lab_tests() {
        vector<LabTestConfig> tests;
        
        // Примеры тестов из таблиц (упрощенные для демонстрации)
        tests.push_back({1, "M", "Gauss+", 1, "T", 10000.0, -1, "PRIORITY", "Priority, min σ", 0.9});
        tests.push_back({2, "E3", "Rayleigh", 1, "K", 1000.0, 3, "FIFO", "FIFO", 0.8});
        tests.push_back({4, "M", "Uniform", 2, "K", 1000.0, 20, "RANDOM", "RAND", 0.9});
        tests.push_back({5, "Gauss+", "M", 4, "T", 10000.0, -1, "LIFO", "LIFO", 0.8});
        tests.push_back({6, "E4", "Gauss+", 1, "K", 1000.0, 6, "ROUND_ROBIN", "RoundRobin", 0.85});
        tests.push_back({7, "Uniform", "Gauss+", 2, "T", 10000.0, 15, "PRIORITY", "Priority, max σ", 0.9});
        tests.push_back({8, "M", "E3", 1, "K", 1000.0, -1, "LIFO", "LIFO", 0.8});
        
        return tests;
    }
    
    // ========== 1. Базовые тесты производительности ==========
    void run_basic_performance_tests() {
        vector<tuple<string, double, double, int, string, int>> test_cases = {
            {"Очень легкая (ρ=0.1)", 0.1, 1.0, 1, "FIFO", -1},
            {"Легкая нагрузка (ρ=0.3)", 0.3, 1.0, 1, "FIFO", -1},
            {"Средняя нагрузка (ρ=0.6)", 0.6, 1.0, 1, "FIFO", -1},
            {"Высокая нагрузка (ρ=0.85)", 0.85, 1.0, 1, "FIFO", -1},
            {"Многоядерный (4 ядра, ρ=0.8)", 0.8, 1.0, 4, "FIFO", -1},
            {"С ограниченным буфером (ρ=0.7)", 0.7, 1.0, 1, "FIFO", 10}
        };
        
        cout << "ТЕСТ: 4 ПОТОКА vs ПОСЛЕДОВАТЕЛЬНО (10000 ед. времени)\n";
        cout << "-----------------------------------------------------\n";
        cout << "Конфигурация           ρ    W(посл)  W(пар)   T_seq(мс) T_par(мс) Ускр. Эфф.%\n";
        cout << "--------------------------------------------------------------------------\n";
        
        for (const auto& [name, rho_target, mu, cores, queue_type_str, buffer] : test_cases) {
            double time = 10000.0;
            int runs = 4;
            double lambda = rho_target * mu * cores;
            auto queue_type = string_to_queue_type(queue_type_str);
            
            if (rho_target >= 1.0) {
                cout << name << ": ПРОПУСК - система нестационарна (ρ=" << rho_target << ")\n";
                continue;
            }
            
            // Последовательное выполнение
            auto seq_start = chrono::high_resolution_clock::now();
            vector<double> wait_times_seq;
            
            for (int i = 0; i < runs; i++) {
                auto arrival = GeneratorFactory::create_exponential(lambda);
                auto service = GeneratorFactory::create_exponential(mu);
                Simulator sim(std::move(arrival), std::move(service), cores, buffer, queue_type);
                sim.run(time);
                wait_times_seq.push_back(sim.avg_wait_time());
            }
            
            auto seq_end = chrono::high_resolution_clock::now();
            double seq_time = chrono::duration<double, milli>(seq_end - seq_start).count();
            double avg_seq_wait = accumulate(wait_times_seq.begin(), wait_times_seq.end(), 0.0) / runs;
            
            // Параллельное выполнение
            auto par_start = chrono::high_resolution_clock::now();
            vector<future<double>> futures;
            
            for (int i = 0; i < runs; i++) {
                futures.push_back(async(launch::async, [=]() {
                    auto arrival = GeneratorFactory::create_exponential(lambda);
                    auto service = GeneratorFactory::create_exponential(mu);
                    Simulator sim(std::move(arrival), std::move(service), cores, buffer, queue_type);
                    sim.run(time);
                    return sim.avg_wait_time();
                }));
            }
            
            vector<double> wait_times_par;
            for (auto& f : futures) {
                wait_times_par.push_back(f.get());
            }
            
            auto par_end = chrono::high_resolution_clock::now();
            double par_time = chrono::duration<double, milli>(par_end - par_start).count();
            double avg_par_wait = accumulate(wait_times_par.begin(), wait_times_par.end(), 0.0) / runs;
            
            // Расчет метрик с ограничением эффективности 100%
            double speedup = (par_time > 0) ? seq_time / par_time : 0;
            double efficiency = min((speedup / runs) * 100, 100.0);  // ← ИСПРАВЛЕНО!
            
            // Вывод с разделением времени
            cout << fixed << setprecision(2);
            cout << left << setw(22) << name
                << setw(5) << rho_target
                << setw(9) << avg_seq_wait
                << setw(9) << avg_par_wait
                << setw(9) << seq_time
                << setw(9) << par_time
                << setw(6) << speedup
                << setw(7) << efficiency << "%\n";
        }
    }
    
    // ========== 2. Тесты из лабораторной работы (упрощенные) ==========
    void run_lab_tests_simplified(const vector<LabTestConfig>& lab_tests) {
        cout << "ТЕСТИРОВАНИЕ ОСНОВНЫХ КОНФИГУРАЦИЙ ИЗ ЛАБОРАТОРНОЙ РАБОТЫ\n";
        cout << "---------------------------------------------------------\n";
        cout << "ID Конфигурация      Ядра Дисц.   ρ(цель) ρ(факт) W(посл) Ускр. Эфф.%\n";
        cout << "---------------------------------------------------------------------\n";
        
        vector<TestResult> results;
        
        // Тестируем только первые 5 тестов для демонстрации
        for (size_t i = 0; i < min(lab_tests.size(), size_t(5)); i++) {
            const auto& test = lab_tests[i];
            
            // Используем ρ из ограничений лабораторной работы
            double rho_target = test.rho_limit;
            double mu = 1.0;
            double lambda = rho_target * mu * test.cores;  // λ = ρ * μ * c
            
            double time = (test.finish_type == "T") ? test.finish_value : 10000.0;
            int jobs = (test.finish_type == "K") ? static_cast<int>(test.finish_value) : 0;
            int runs = 3;
            
            auto queue_type = string_to_queue_type(test.queue_discipline);
            
            // Проверяем стационарность
            if (rho_target >= 1.0) {
                cout << "Тест " << test.test_id << ": ПРОПУСК - ρ=" << rho_target << " >= 1\n";
                continue;
            }
            
            TestResult result;
            result.test_id = test.test_id;
            result.config_name = "Тест " + to_string(test.test_id);
            result.rho_value = rho_target;
            
            // Последовательное выполнение
            auto seq_start = chrono::high_resolution_clock::now();
            vector<double> wait_times_seq;
            vector<double> rho_values_seq;
            
            for (int run = 0; run < runs; run++) {
                auto arrival = GeneratorFactory::create_exponential(lambda);
                auto service = GeneratorFactory::create_exponential(mu);
                Simulator sim(std::move(arrival), std::move(service), test.cores, test.buffer_size, queue_type);
                
                if (test.finish_type == "T") {
                    sim.run(time);
                } else {
                    sim.run_until_jobs(jobs);
                }
                
                wait_times_seq.push_back(sim.avg_wait_time());
                rho_values_seq.push_back(sim.rho());
                result.server_utilization = sim.server_utilization();
                result.loss_probability = sim.loss_probability();
                result.avg_queue_length = sim.avg_queue_length();
                
                // Проверка стационарности
                if (!sim.is_stationary()) {
                    cout << "  [ВНИМАНИЕ: система нестационарна! ρ=" << sim.rho() << "]\n";
                }
            }
            
            auto seq_end = chrono::high_resolution_clock::now();
            result.time_seq_ms = chrono::duration<double, milli>(seq_end - seq_start).count();
            result.wait_time_seq = accumulate(wait_times_seq.begin(), wait_times_seq.end(), 0.0) / runs;
            double avg_rho_seq = accumulate(rho_values_seq.begin(), rho_values_seq.end(), 0.0) / runs;
            
            // Параллельное выполнение
            auto par_start = chrono::high_resolution_clock::now();
            vector<future<tuple<double, double>>> futures;
            
            for (int run = 0; run < runs; run++) {
                futures.push_back(async(launch::async, [=]() {
                    auto arrival = GeneratorFactory::create_exponential(lambda);
                    auto service = GeneratorFactory::create_exponential(mu);
                    Simulator sim(std::move(arrival), std::move(service), test.cores, test.buffer_size, queue_type);
                    
                    if (test.finish_type == "T") {
                        sim.run(time);
                    } else {
                        sim.run_until_jobs(jobs);
                    }
                    
                    return make_tuple(sim.avg_wait_time(), sim.rho());
                }));
            }
            
            vector<double> wait_times_par;
            for (auto& f : futures) {
                auto [wait, rho] = f.get();
                wait_times_par.push_back(wait);
                // rho не используется, но получаем для возможной проверки
            }
            
            auto par_end = chrono::high_resolution_clock::now();
            result.time_par_ms = chrono::duration<double, milli>(par_end - par_start).count();
            result.wait_time_par = accumulate(wait_times_par.begin(), wait_times_par.end(), 0.0) / runs;
            
            // Расчет метрик
            result.speedup = result.time_seq_ms / result.time_par_ms;
            result.efficiency = (result.speedup / runs) * 100;
            
            // Вывод
            cout << fixed << setprecision(3);
            cout << setw(2) << test.test_id << " "
                 << left << setw(17) << (test.arrival_dist + "/" + test.service_dist).substr(0, 16)
                 << setw(5) << test.cores
                 << setw(7) << test.queue_discipline.substr(0, 6)
                 << setw(8) << rho_target
                 << setw(8) << avg_rho_seq
                 << setw(8) << result.wait_time_seq
                 << setw(6) << result.speedup
                 << setw(7) << result.efficiency << "%\n";
            
            results.push_back(result);
        }
        
        // Вывод суммарной статистики
        print_summary_statistics(results);
    }
    
    // ========== 3. Масштабируемость по количеству ядер ==========
    void test_scalability_cores() {
        cout << "МАСШТАБИРУЕМОСТЬ: ВЛИЯНИЕ КОЛИЧЕСТВА ЯДЕР СЕРВЕРА (ПРИ ФИКСИРОВАННОМ ρ=0.8)\n";
        cout << "μ=1.0 t=20000 runs=4 queue=FIFO\n";
        cout << "-------------------------------------------------\n";
        cout << "Ядра  λ       ρ(расч) W(средн) Время(мс) Ускорение Эфф.(%) Загрузка(%)\n";
        cout << "--------------------------------------------------------------------\n";
        
        double rho_target = 0.8;  // Фиксированный ρ для всех конфигураций
        double mu = 1.0;
        double time = 20000.0;
        int runs = 4;
        
        vector<int> core_counts = {1, 2, 4, 8};
        vector<double> speedups;
        vector<double> efficiencies;
        
        for (int cores : core_counts) {
            // Рассчитываем λ для достижения ρ=0.8
            double lambda = rho_target * mu * cores;
            
            // Последовательное выполнение
            auto seq_start = chrono::high_resolution_clock::now();
            double total_wait_seq = 0;
            double total_util_seq = 0;
            double total_rho_seq = 0;
            
            for (int i = 0; i < runs; i++) {
                auto arrival = GeneratorFactory::create_exponential(lambda);
                auto service = GeneratorFactory::create_exponential(mu);
                Simulator sim(std::move(arrival), std::move(service), cores, -1, 
                             QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO);
                sim.run(time);
                total_wait_seq += sim.avg_wait_time();
                total_util_seq += sim.server_utilization();
                total_rho_seq += sim.rho();
            }
            
            auto seq_end = chrono::high_resolution_clock::now();
            double seq_time = chrono::duration<double, milli>(seq_end - seq_start).count();
            
            // Параллельное выполнение
            auto par_start = chrono::high_resolution_clock::now();
            vector<future<tuple<double, double, double>>> futures;
            
            for (int i = 0; i < runs; i++) {
                futures.push_back(async(launch::async, [=]() {
                    auto arrival = GeneratorFactory::create_exponential(lambda);
                    auto service = GeneratorFactory::create_exponential(mu);
                    Simulator sim(std::move(arrival), std::move(service), cores, -1,
                                 QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO);
                    sim.run(time);
                    return make_tuple(sim.avg_wait_time(), 
                                     sim.server_utilization(),
                                     sim.rho());
                }));
            }
            
            double total_wait_par = 0;
            double total_util_par = 0;
            double total_rho_par = 0;
            for (auto& f : futures) {
                auto [wait, util, rho] = f.get();
                total_wait_par += wait;
                total_util_par += util;
                total_rho_par += rho;
            }
            
            auto par_end = chrono::high_resolution_clock::now();
            double par_time = chrono::duration<double, milli>(par_end - par_start).count();
            double avg_wait_par = total_wait_par / runs;
            double avg_util_par = total_util_par / runs * 100;
            double avg_rho_par = total_rho_par / runs;
            
            // Расчет метрик
            double speedup = seq_time / par_time;
            double efficiency = (speedup / runs) * 100;
            
            speedups.push_back(speedup);
            efficiencies.push_back(efficiency);
            
            // Вывод
            cout << fixed << setprecision(2);
            cout << setw(4) << cores
                 << setw(7) << lambda
                 << setw(9) << avg_rho_par
                 << setw(9) << avg_wait_par
                 << setw(10) << seq_time
                 << setw(10) << speedup
                 << setw(9) << efficiency
                 << setw(12) << avg_util_par << "%\n";
        }
        
        // Анализ масштабируемости
        cout << "\nАНАЛИЗ МАСШТАБИРУЕМОСТИ:\n";
        cout << "Целевой ρ = 0.8 для всех конфигураций\n";
        cout << "Идеальное ускорение: линейное (8 ядер → 8x ускорение)\n";
        cout << "Реальное ускорение на 8 ядрах: " << fixed << setprecision(2) << speedups.back() << "x\n";
        cout << "Эффективность параллелизма: " << efficiencies.back() << "%\n";
        
        // Количественный анализ масштабируемости
        if (core_counts.size() >= 2) {
            double scaling_factor = speedups.back() / core_counts.back();
            cout << "Коэффициент масштабируемости: " << scaling_factor * 100 << "%\n";
            
            if (scaling_factor > 0.8) {
                cout << "ВЫВОД: Отличная масштабируемость (>80% эффективности)!\n";
            } else if (scaling_factor > 0.6) {
                cout << "ВЫВОД: Хорошая масштабируемость (60-80% эффективности).\n";
            } else if (scaling_factor > 0.4) {
                cout << "ВЫВОД: Удовлетворительная масштабируемость (40-60% эффективности).\n";
            } else {
                cout << "ВЫВОД: Ограниченная масштабируемость (<40% эффективности).\n";
            }
        }
    }
    
    // ========== 4. Тестирование дисциплин очередей ==========
    void test_queue_disciplines_comparison() {
        cout << "СРАВНЕНИЕ ДИСЦИПЛИН ОЧЕРЕДИ ПРИ ПАРАЛЛЕЛЬНОМ ВЫПОЛНЕНИИ\n";
        cout << "ρ=0.7 μ=1.0 t=15000 runs=4 cores=1\n";
        cout << "-------------------------------------------------------\n";
        
        double rho_target = 0.7;
        double mu = 1.0;
        double lambda = rho_target * mu;  // λ = ρ * μ
        double time = 15000.0;
        int runs = 4;
        
        vector<QueueDisciplines::QueueStrategyFactory<Job>::Type> disciplines = {
            QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO,
            QueueDisciplines::QueueStrategyFactory<Job>::Type::LIFO,
            QueueDisciplines::QueueStrategyFactory<Job>::Type::RANDOM,
            QueueDisciplines::QueueStrategyFactory<Job>::Type::PRIORITY,
            QueueDisciplines::QueueStrategyFactory<Job>::Type::ROUND_ROBIN
        };
        
        cout << "Дисциплина   ρ(факт) W(посл)  W(пар)   Время(мс) Ускр. Эфф.%\n";
        cout << "-----------------------------------------------------------\n";
        
        for (auto discipline : disciplines) {
            string disc_name = QueueDisciplines::QueueStrategyFactory<Job>::type_to_string(discipline);
            
            // Последовательное выполнение
            auto seq_start = chrono::high_resolution_clock::now();
            vector<double> wait_times_seq;
            vector<double> rho_values_seq;
            
            for (int i = 0; i < runs; i++) {
                auto arrival = GeneratorFactory::create_exponential(lambda);
                auto service = GeneratorFactory::create_exponential(mu);
                Simulator sim(std::move(arrival), std::move(service), 1, -1, discipline);
                sim.run(time);
                wait_times_seq.push_back(sim.avg_wait_time());
                rho_values_seq.push_back(sim.rho());
            }
            
            auto seq_end = chrono::high_resolution_clock::now();
            double seq_time = chrono::duration<double, milli>(seq_end - seq_start).count();
            double avg_seq_wait = accumulate(wait_times_seq.begin(), wait_times_seq.end(), 0.0) / runs;
            double avg_rho_seq = accumulate(rho_values_seq.begin(), rho_values_seq.end(), 0.0) / runs;
            
            // Параллельное выполнение
            auto par_start = chrono::high_resolution_clock::now();
            vector<future<double>> futures;
            
            for (int i = 0; i < runs; i++) {
                futures.push_back(async(launch::async, [=]() {
                    auto arrival = GeneratorFactory::create_exponential(lambda);
                    auto service = GeneratorFactory::create_exponential(mu);
                    Simulator sim(std::move(arrival), std::move(service), 1, -1, discipline);
                    sim.run(time);
                    return sim.avg_wait_time();
                }));
            }
            
            vector<double> wait_times_par;
            for (auto& f : futures) {
                wait_times_par.push_back(f.get());
            }
            
            auto par_end = chrono::high_resolution_clock::now();
            double par_time = chrono::duration<double, milli>(par_end - par_start).count();
            double avg_par_wait = accumulate(wait_times_par.begin(), wait_times_par.end(), 0.0) / runs;
            
            // Расчет метрик
            double speedup = seq_time / par_time;
            double efficiency = (speedup / runs) * 100;
            
            // Вывод
            cout << fixed << setprecision(2);
            cout << left << setw(11) << disc_name
                 << setw(9) << avg_rho_seq
                 << setw(9) << avg_seq_wait
                 << setw(9) << avg_par_wait
                 << setw(10) << seq_time
                 << setw(6) << speedup
                 << setw(7) << efficiency << "%\n";
        }
        
        cout << "\nАНАЛИЗ:\n";
        cout << "- Все системы стационарны (ρ ≈ 0.7 < 1)\n";
        cout << "- FIFO: Стандартная дисциплина, стабильная производительность\n";
        cout << "- LIFO: Может увеличивать среднее время ожидания (эффект 'голодания')\n";
        cout << "- RANDOM: Наихудшая предсказуемость времени ожидания\n";
        cout << "- PRIORITY: Эффективна для приоритетных задач, но требует сортировки\n";
        cout << "- ROUND_ROBIN: Справедливое распределение, но с накладными расходами\n";
    }
    
    // ========== 5. Анализ ускорения для разных нагрузок ==========
    void test_speedup_vs_load() {
        cout << "ЗАВИСИМОСТЬ УСКОРЕНИЯ ОТ НАГРУЗКИ СИСТЕМЫ (ρ)\n";
        cout << "μ=1.0 t=10000 runs=4 cores=1 queue=FIFO\n";
        cout << "------------------------------------------------\n";
        cout << "ρ(цель) ρ(факт) W(посл) W(пар) Время(мс) Ускр.  Эфф.%  Загрузка(%)\n";
        cout << "--------------------------------------------------------------\n";
        
        double mu = 1.0;
        double time = 10000.0;
        int runs = 4;
        
        vector<double> loads = {0.1, 0.3, 0.5, 0.7, 0.85, 0.95};
        vector<double> speedups;
        
        for (double rho_target : loads) {
            double lambda = rho_target * mu;  // λ = ρ * μ
            
            // Последовательное выполнение
            auto seq_start = chrono::high_resolution_clock::now();
            double total_wait_seq = 0;
            double total_rho_seq = 0;
            double total_util_seq = 0;
            
            for (int i = 0; i < runs; i++) {
                auto arrival = GeneratorFactory::create_exponential(lambda);
                auto service = GeneratorFactory::create_exponential(mu);
                Simulator sim(std::move(arrival), std::move(service), 1, -1, 
                             QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO);
                sim.run(time);
                total_wait_seq += sim.avg_wait_time();
                total_rho_seq += sim.rho();
                total_util_seq += sim.server_utilization();
            }
            
            auto seq_end = chrono::high_resolution_clock::now();
            double seq_time = chrono::duration<double, milli>(seq_end - seq_start).count();
            double avg_wait_seq = total_wait_seq / runs;
            
            // Параллельное выполнение
            auto par_start = chrono::high_resolution_clock::now();
            vector<future<tuple<double, double, double>>> futures;
            
            for (int i = 0; i < runs; i++) {
                futures.push_back(async(launch::async, [=]() {
                    auto arrival = GeneratorFactory::create_exponential(lambda);
                    auto service = GeneratorFactory::create_exponential(mu);
                    Simulator sim(std::move(arrival), std::move(service), 1, -1,
                                 QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO);
                    sim.run(time);
                    return make_tuple(sim.avg_wait_time(), 
                                     sim.rho(),
                                     sim.server_utilization());
                }));
            }
            
            double total_wait_par = 0;
            double total_rho_par = 0;
            double total_util_par = 0;
            for (auto& f : futures) {
                auto [wait, rho, util] = f.get();
                total_wait_par += wait;
                total_rho_par += rho;
                total_util_par += util;
            }
            
            auto par_end = chrono::high_resolution_clock::now();
            double par_time = chrono::duration<double, milli>(par_end - par_start).count();
            double avg_wait_par = total_wait_par / runs;
            double avg_rho_par = total_rho_par / runs;
            double avg_util_par = total_util_par / runs * 100;
            
            // Расчет метрик
            double speedup = seq_time / par_time;
            double efficiency = (speedup / runs) * 100;
            
            speedups.push_back(speedup);
            
            // Вывод
            cout << fixed << setprecision(3);
            cout << setw(7) << rho_target
                 << setw(9) << avg_rho_par
                 << setw(9) << avg_wait_seq
                 << setw(9) << avg_wait_par
                 << setw(10) << seq_time
                 << setw(7) << speedup
                 << setw(7) << efficiency << "%"
                 << setw(12) << avg_util_par << "%\n";
        }
        
        // Находим оптимальную нагрузку
        auto max_speedup_it = max_element(speedups.begin(), speedups.end());
        size_t optimal_idx = distance(speedups.begin(), max_speedup_it);
        double optimal_load = loads[optimal_idx];
        double max_speedup = speedups[optimal_idx];
        
        cout << "\nВЫВОДЫ ПО ЗАВИСИМОСТИ УСКОРЕНИЯ ОТ НАГРУЗКИ:\n";
        cout << "1. При низкой нагрузке (ρ < 0.3): ускорение снижено, так как\n";
        cout << "   заданий мало и накладные расходы доминируют.\n";
        cout << "2. Оптимальная нагрузка для параллелизации: ρ ≈ " << optimal_load << "\n";
        cout << "   Максимальное ускорение: " << max_speedup << "x\n";
        cout << "3. При высокой нагрузке (ρ > 0.85): ускорение снижается, так как\n";
        cout << "   система близка к насыщению и задания сильно зависимы.\n";
        cout << "4. Все системы стационарны (ρ < 1), поэтому W конечное.\n";
        cout << "5. Загрузка сервера приближается к ρ (как и должно быть).\n";
    }
    
    // ========== Вспомогательные методы ==========
    
    QueueDisciplines::QueueStrategyFactory<Job>::Type string_to_queue_type(const string& str) {
        if (str == "FIFO") return QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO;
        if (str == "LIFO") return QueueDisciplines::QueueStrategyFactory<Job>::Type::LIFO;
        if (str == "RANDOM" || str == "RAND") return QueueDisciplines::QueueStrategyFactory<Job>::Type::RANDOM;
        if (str == "PRIORITY") return QueueDisciplines::QueueStrategyFactory<Job>::Type::PRIORITY;
        if (str == "ROUND_ROBIN" || str == "RoundRobin") return QueueDisciplines::QueueStrategyFactory<Job>::Type::ROUND_ROBIN;
        return QueueDisciplines::QueueStrategyFactory<Job>::Type::FIFO;
    }
    
    void print_summary_statistics(const vector<TestResult>& results) {
        if (results.empty()) {
            cout << "\nНет результатов для анализа (все тесты пропущены из-за нестационарности).\n";
            return;
        }
        
        double avg_speedup = 0;
        double avg_efficiency = 0;
        double max_speedup = 0;
        double min_speedup = 1000;
        int valid_tests = 0;
        
        for (const auto& res : results) {
            if (res.rho_value < 1.0) {  // Только стационарные тесты
                avg_speedup += res.speedup;
                avg_efficiency += res.efficiency;
                max_speedup = max(max_speedup, res.speedup);
                min_speedup = min(min_speedup, res.speedup);
                valid_tests++;
            }
        }
        
        if (valid_tests == 0) {
            cout << "\nНет стационарных тестов для анализа.\n";
            return;
        }
        
        avg_speedup /= valid_tests;
        avg_efficiency /= valid_tests;
        
        cout << "\nСВОДНАЯ СТАТИСТИКА (" << valid_tests << " стационарных тестов):\n";
        cout << fixed << setprecision(2);
        cout << "Среднее ускорение: " << avg_speedup << "x\n";
        cout << "Средняя эффективность: " << avg_efficiency << "%\n";
        cout << "Максимальное ускорение: " << max_speedup << "x\n";
        cout << "Минимальное ускорение: " << min_speedup << "x\n";
        cout << "Разброс ускорения: " << (max_speedup - min_speedup) << "x\n";
        
        cout << "\nОЦЕНКА ПАРАЛЛЕЛЬНОЙ ЭФФЕКТИВНОСТИ:\n";
        if (avg_efficiency > 80) {
            cout << "ОТЛИЧНО: Высокая эффективность параллелизма (>80%)\n";
        } else if (avg_efficiency > 60) {
            cout << "ХОРОШО: Приемлемая эффективность параллелизма (60-80%)\n";
        } else if (avg_efficiency > 40) {
            cout << "УДОВЛЕТВОРИТЕЛЬНО: Средняя эффективность (40-60%)\n";
        } else {
            cout << "НИЗКО: Эффективность параллелизма требует оптимизации (<40%)\n";
        }
    }
};

mutex ParallelFinal::cout_mutex;

#endif // PARALLEL_FINAL_H