#include "simulator.h"
#include <iostream>
#include <iomanip>
#include <memory>

using namespace std;

/**
 * Демонстрация работы симулятора с системой M/M/1
 */
void demo_mm1_system() {
    cout << "\n" << string(60, '=') << "\n";
    cout << "ДЕМОНСТРАЦИЯ: СИСТЕМА M/M/1\n";
    cout << string(60, '=') << "\n";
    
    // Параметры системы
    double lambda = 0.8;  // интенсивность прибытий
    double mu = 1.0;      // интенсивность обслуживания
    
    cout << "\nПараметры системы:\n";
    cout << "  Интенсивность прибытий λ = " << lambda << "\n";
    cout << "  Интенсивность обслуживания μ = " << mu << "\n";
    cout << "  Загрузка ρ = λ/μ = " << lambda/mu << "\n\n";
    
    // Создаем генераторы
    auto arrival_gen = GeneratorFactory::create_exponential(lambda);
    auto service_gen = GeneratorFactory::create_exponential(mu);
    
    // Создаем симулятор (1 ядро, бесконечный буфер)
    Simulator simulator(std::move(arrival_gen), std::move(service_gen), 1, -1);
    
    // Выводим конфигурацию
    simulator.print_configuration();
    
    // Запускаем моделирование
    simulator.run(10000.0);  // на 10000 единиц времени
    
    // Выводим результаты
    simulator.print_statistics();
}

/**
 * Сравнение систем с разным количеством ядер
 */
void compare_multi_core_systems() {
    cout << "\n" << string(60, '=') << "\n";
    cout << "СРАВНЕНИЕ: ВЛИЯНИЕ КОЛИЧЕСТВА ЯДЕР\n";
    cout << string(60, '=') << "\n";
    
    double lambda = 2.0;  // общая интенсивность прибытий
    double mu = 1.0;      // интенсивность обслуживания на одном ядре
    
    cout << "\nПараметры:\n";
    cout << "  Общая λ = " << lambda << ", μ на ядро = " << mu << "\n\n";
    
    cout << left << setw(10) << "Ядра" 
         << setw(15) << "Загрузка ρ" 
         << setw(15) << "W среднее" 
         << setw(15) << "Потери %" 
         << "\n";
    cout << string(55, '-') << "\n";
    
    for (int cores = 1; cores <= 4; cores++) {
        // Для сравнения сохраняем общую интенсивность прибытий
        auto arrival_gen = GeneratorFactory::create_exponential(lambda);
        auto service_gen = GeneratorFactory::create_exponential(mu);
        
        Simulator simulator(std::move(arrival_gen), std::move(service_gen), cores, 10);
        
        // Краткое моделирование
        simulator.run(2000.0);
        
        cout << fixed << setprecision(4);
        cout << left << setw(10) << cores
             << setw(15) << simulator.rho()
             << setw(15) << simulator.avg_wait_time()
             << setw(15) << simulator.loss_probability() * 100
             << "\n";
    }
}

/**
 * Тестирование разных распределений времени обслуживания
 */
void test_different_distributions() {
    cout << "\n" << string(60, '=') << "\n";
    cout << "СРАВНЕНИЕ: РАЗНЫЕ РАСПРЕДЕЛЕНИЯ ОБСЛУЖИВАНИЯ\n";
    cout << string(60, '=') << "\n";
    
    double lambda = 0.5;
    double mean_service_time = 1.0;  // одинаковое среднее для всех распределений
    
    cout << "\nλ = " << lambda << ", среднее время обслуживания = " << mean_service_time << "\n\n";
    
    cout << left << setw(20) << "Распределение" 
         << setw(15) << "Дисперсия" 
         << setw(15) << "W среднее" 
         << setw(15) << "W дисперсия" 
         << "\n";
    cout << string(65, '-') << "\n";
    
    // Тестируем каждое распределение отдельно
    
    // 1. Exponential
    {
        auto arrival_gen = GeneratorFactory::create_exponential(lambda);
        auto service_gen = GeneratorFactory::create_exponential(1.0/mean_service_time);
        
        // Сохраняем параметры ДО передачи
        double variance = service_gen->variance();
        
        Simulator simulator(std::move(arrival_gen), std::move(service_gen), 1, -1);
        simulator.run(5000.0);
        
        cout << fixed << setprecision(4);
        cout << left << setw(20) << "Exponential"
             << setw(15) << variance
             << setw(15) << simulator.avg_wait_time()
             << setw(15) << simulator.wait_time_variance()
             << "\n";
    }
    
    // 2. Uniform
    {
        auto arrival_gen = GeneratorFactory::create_exponential(lambda);
        auto service_gen = GeneratorFactory::create_uniform(0.5, 1.5);
        
        // Сохраняем параметры ДО передачи
        double variance = service_gen->variance();
        
        Simulator simulator(std::move(arrival_gen), std::move(service_gen), 1, -1);
        simulator.run(5000.0);
        
        cout << left << setw(20) << "Uniform[0.5,1.5]"
             << setw(15) << variance
             << setw(15) << simulator.avg_wait_time()
             << setw(15) << simulator.wait_time_variance()
             << "\n";
    }
    
    // 3. Deterministic
    {
        auto arrival_gen = GeneratorFactory::create_exponential(lambda);
        auto service_gen = GeneratorFactory::create_deterministic(mean_service_time);
        
        // Сохраняем параметры ДО передачи
        double variance = service_gen->variance();
        
        Simulator simulator(std::move(arrival_gen), std::move(service_gen), 1, -1);
        simulator.run(5000.0);
        
        cout << left << setw(20) << "Deterministic"
             << setw(15) << variance
             << setw(15) << simulator.avg_wait_time()
             << setw(15) << simulator.wait_time_variance()
             << "\n";
    }
    
    // 4. Erlang
    {
        auto arrival_gen = GeneratorFactory::create_exponential(lambda);
        auto service_gen = GeneratorFactory::create_erlang(2, 2.0/mean_service_time);
        
        // Сохраняем параметры ДО передачи
        double variance = service_gen->variance();
        
        Simulator simulator(std::move(arrival_gen), std::move(service_gen), 1, -1);
        simulator.run(5000.0);
        
        cout << left << setw(20) << "Erlang(k=2)"
             << setw(15) << variance
             << setw(15) << simulator.avg_wait_time()
             << setw(15) << simulator.wait_time_variance()
             << "\n";
    }
    
    cout << "\nВывод: при одинаковом среднем, дисперсия времени обслуживания\n";
    cout << "существенно влияет на среднее время ожидания.\n";
}

int main() {
    cout << "СИМУЛЯТОР СИСТЕМ МАССОВОГО ОБСЛУЖИВАНИЯ\n";
    cout << "Событийно-ориентированное моделирование\n";
    cout << string(60, '=') << "\n";
    
    try {
        // 1. Демонстрация базовой системы M/M/1
        demo_mm1_system();
        
        // 2. Сравнение многопоточных систем
        compare_multi_core_systems();
        
        // 3. Тестирование разных распределений
        test_different_distributions();
        
        // Сохранение результатов последнего теста
        {
            auto arrival_gen = GeneratorFactory::create_exponential(0.8);
            auto service_gen = GeneratorFactory::create_exponential(1.0);
            Simulator simulator(std::move(arrival_gen), std::move(service_gen), 1, -1);
            simulator.run(1000.0);
            simulator.save_statistics("results.csv");
        }
        
    } catch (const exception& e) {
        cerr << "\nОШИБКА: " << e.what() << endl;
        return 1;
    }
    
    cout << "\n" << string(60, '=') << "\n";
    cout << "МОДЕЛИРОВАНИЕ ЗАВЕРШЕНО\n";
    cout << string(60, '=') << "\n";
    
    return 0;
}