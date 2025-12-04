#ifndef RANDOM_GENERATOR_H
#define RANDOM_GENERATOR_H

#include <random>
#include <memory>
#include <cmath>
#include <string>
#include <stdexcept>

/**
 * Абстрактный базовый класс генератора случайных чисел
 * для событийно-ориентированного моделирования
 */
class RandomGenerator {
public:
    virtual ~RandomGenerator() = default;
    
    virtual double generate() = 0;           // генерация числа
    virtual double mean() const = 0;         // математическое ожидание
    virtual double variance() const = 0;     // дисперсия
    virtual std::string name() const = 0;    // название распределения
    virtual std::unique_ptr<RandomGenerator> clone() const = 0;
};

// ==================== КОНКРЕТНЫЕ РАСПРЕДЕЛЕНИЯ ====================

/**
 * Экспоненциальное распределение (поток Пуассона)
 * A(x) = 1 - exp(-λx)
 */
class ExponentialGenerator : public RandomGenerator {
private:
    double lambda_;
    std::mt19937_64 generator_;
    std::exponential_distribution<double> distribution_;
    
public:
    ExponentialGenerator(double lambda) 
        : lambda_(lambda), distribution_(lambda) {
        if (lambda <= 0) {
            throw std::invalid_argument("Параметр λ должен быть положительным");
        }
        std::random_device rd;
        generator_.seed(rd());
    }
    
    double generate() override {
        return distribution_(generator_);
    }
    
    double mean() const override {
        return 1.0 / lambda_;
    }
    
    double variance() const override {
        return 1.0 / (lambda_ * lambda_);
    }
    
    std::string name() const override {
        return "Exponential(λ=" + std::to_string(lambda_) + ")";
    }
    
    std::unique_ptr<RandomGenerator> clone() const override {
        auto clone = std::make_unique<ExponentialGenerator>(lambda_);
        clone->generator_ = generator_;
        return clone;
    }
};

/**
 * Равномерное распределение на отрезке [a, b]
 */
class UniformGenerator : public RandomGenerator {
private:
    double a_, b_;
    std::mt19937_64 generator_;
    std::uniform_real_distribution<double> distribution_;
    
public:
    UniformGenerator(double a, double b) 
        : a_(a), b_(b), distribution_(a, b) {
        if (a >= b) {
            throw std::invalid_argument("a должен быть меньше b");
        }
        std::random_device rd;
        generator_.seed(rd());
    }
    
    double generate() override {
        return distribution_(generator_);
    }
    
    double mean() const override {
        return (a_ + b_) / 2.0;
    }
    
    double variance() const override {
        return ((b_ - a_) * (b_ - a_)) / 12.0;
    }
    
    std::string name() const override {
        return "Uniform[" + std::to_string(a_) + "," + std::to_string(b_) + "]";
    }
    
    std::unique_ptr<RandomGenerator> clone() const override {
        auto clone = std::make_unique<UniformGenerator>(a_, b_);
        clone->generator_ = generator_;
        return clone;
    }
};

/**
 * Детерминированное (постоянное) распределение
 */
class DeterministicGenerator : public RandomGenerator {
private:
    double value_;
    
public:
    DeterministicGenerator(double value) : value_(value) {}
    
    double generate() override { 
        return value_; 
    }
    
    double mean() const override { 
        return value_; 
    }
    
    double variance() const override { 
        return 0.0; 
    }
    
    std::string name() const override {
        return "Deterministic(" + std::to_string(value_) + ")";
    }
    
    std::unique_ptr<RandomGenerator> clone() const override {
        return std::make_unique<DeterministicGenerator>(value_);
    }
};

/**
 * Распределение Эрланга порядка k
 * Сумма k независимых экспоненциальных величин
 */
class ErlangGenerator : public RandomGenerator {
private:
    int k_;
    double lambda_;
    std::mt19937_64 generator_;
    std::exponential_distribution<double> exp_dist_;
    
public:
    ErlangGenerator(int k, double lambda) 
        : k_(k), lambda_(lambda), exp_dist_(lambda) {
        if (k <= 0) throw std::invalid_argument("k должен быть положительным");
        if (lambda <= 0) throw std::invalid_argument("λ должен быть положительным");
        
        std::random_device rd;
        generator_.seed(rd());
    }
    
    double generate() override {
        double sum = 0.0;
        for (int i = 0; i < k_; i++) {
            sum += exp_dist_(generator_);
        }
        return sum;
    }
    
    double mean() const override {
        return k_ / lambda_;
    }
    
    double variance() const override {
        return k_ / (lambda_ * lambda_);
    }
    
    std::string name() const override {
        return "Erlang(k=" + std::to_string(k_) + ", λ=" + std::to_string(lambda_) + ")";
    }
    
    std::unique_ptr<RandomGenerator> clone() const override {
        auto clone = std::make_unique<ErlangGenerator>(k_, lambda_);
        clone->generator_ = generator_;
        return clone;
    }
};

/**
 * Фабрика для создания генераторов
 */
class GeneratorFactory {
public:
    static std::unique_ptr<RandomGenerator> create_exponential(double lambda) {
        return std::make_unique<ExponentialGenerator>(lambda);
    }
    
    static std::unique_ptr<RandomGenerator> create_uniform(double a, double b) {
        return std::make_unique<UniformGenerator>(a, b);
    }
    
    static std::unique_ptr<RandomGenerator> create_deterministic(double value) {
        return std::make_unique<DeterministicGenerator>(value);
    }
    
    static std::unique_ptr<RandomGenerator> create_erlang(int k, double lambda) {
        return std::make_unique<ErlangGenerator>(k, lambda);
    }
};

#endif // RANDOM_GENERATOR_H