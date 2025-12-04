#ifndef DISTRIBUTIONS_H
#define DISTRIBUTIONS_H

#include "random_generator.h"
#include <string>
#include <memory>
#include <vector>

namespace TestDistributions {

// Структура для конфигурации распределения
struct DistributionConfig {
    std::string name;
    std::unique_ptr<RandomGenerator> arrival_gen;
    std::unique_ptr<RandomGenerator> service_gen;
    double mean_service_time;  // Среднее время обслуживания
    double arrival_rate;       // Интенсивность прибытий
    
    DistributionConfig(std::string n, 
                      std::unique_ptr<RandomGenerator> arr,
                      std::unique_ptr<RandomGenerator> serv,
                      double mean_st = 1.0,
                      double arr_rate = 0.8)
        : name(std::move(n))
        , arrival_gen(std::move(arr))
        , service_gen(std::move(serv))
        , mean_service_time(mean_st)
        , arrival_rate(arr_rate) {}
};

// Коллекция тестовых распределений
class DistributionCollection {
public:
    static std::vector<DistributionConfig> get_all_distributions(double lambda = 0.8) {
        std::vector<DistributionConfig> distributions;
        
        double mu = 1.0;  // Среднее время обслуживания = 1
        
        // 1. Экспоненциальное (классическое M/M/1)
        distributions.emplace_back(
            "Exponential (M/M/1)",
            GeneratorFactory::create_exponential(lambda),
            GeneratorFactory::create_exponential(mu),
            1.0,
            lambda
        );
        
        // 2. Равномерное (M/U/1)
        double a = 0.5;  // минимум
        double b = 1.5;  // максимум
        double uniform_mean = (a + b) / 2.0;
        distributions.emplace_back(
            "Uniform [0.5,1.5] (M/U/1)",
            GeneratorFactory::create_exponential(lambda),
            GeneratorFactory::create_uniform(a, b),
            uniform_mean,
            lambda
        );
        
        // 3. Детерминированное (M/D/1)
        distributions.emplace_back(
            "Deterministic (M/D/1)",
            GeneratorFactory::create_exponential(lambda),
            GeneratorFactory::create_deterministic(1.0),
            1.0,
            lambda
        );
        
        // 4. Эрланга (M/Ek/1)
        int k = 2;
        double erlang_lambda = k / mu;  // Для сохранения среднего = 1
        distributions.emplace_back(
            "Erlang(k=2) (M/E2/1)",
            GeneratorFactory::create_exponential(lambda),
            GeneratorFactory::create_erlang(k, erlang_lambda),
            1.0,
            lambda
        );
        
        // 5. Гиперэкспоненциальное (приближение)
        // Смесь двух экспоненциальных с разными λ
        distributions.emplace_back(
            "Hyper-Exponential (M/H2/1)",
            GeneratorFactory::create_exponential(lambda),
            GeneratorFactory::create_exponential(1.5), // Более "размазанное"
            1.0,
            lambda
        );
        
        return distributions;
    }
    
    // Получить только экспоненциальное (для быстрых тестов)
    static DistributionConfig get_exponential(double lambda = 0.8) {
        return DistributionConfig(
            "Exponential",
            GeneratorFactory::create_exponential(lambda),
            GeneratorFactory::create_exponential(1.0),
            1.0,
            lambda
        );
    }
    
    // Получить распределения для нагрузки
    static std::vector<DistributionConfig> get_for_load_testing() {
        std::vector<DistributionConfig> configs;
        std::vector<double> loads = {0.3, 0.6, 0.9, 1.2};
        
        for (double load : loads) {
            std::string name = "Exp (ρ=" + std::to_string(load).substr(0, 3) + ")";
            configs.emplace_back(
                name,
                GeneratorFactory::create_exponential(load), // λ = ρ * μ, где μ=1
                GeneratorFactory::create_exponential(1.0),
                1.0,
                load
            );
        }
        
        return configs;
    }
};

} // namespace TestDistributions

#endif // DISTRIBUTIONS_H