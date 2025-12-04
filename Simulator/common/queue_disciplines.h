#ifndef QUEUE_DISCIPLINES_H
#define QUEUE_DISCIPLINES_H

#include <stdexcept>
#include <vector>
#include <queue>
#include <memory>
#include <random>
#include <algorithm>
#include <string>

namespace QueueDisciplines {

// Базовый класс для дисциплины очереди
template<typename T>
class QueueStrategy {
public:
    virtual ~QueueStrategy() = default;
    virtual void push(const T& item) = 0;
    virtual T pop() = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
    virtual std::string name() const = 0;
    virtual std::unique_ptr<QueueStrategy<T>> clone() const = 0;
};

// 1. FIFO (First-In-First-Out) - стандартная очередь
template<typename T>
class FIFOStrategy : public QueueStrategy<T> {
private:
    std::queue<T> queue_;
    
public:
    void push(const T& item) override {
        queue_.push(item);
    }
    
    T pop() override {
        if (queue_.empty()) {
            throw std::runtime_error("Queue is empty");
        }
        T item = queue_.front();
        queue_.pop();
        return item;
    }
    
    bool empty() const override {
        return queue_.empty();
    }
    
    size_t size() const override {
        return queue_.size();
    }
    
    std::string name() const override {
        return "FIFO";
    }
    
    std::unique_ptr<QueueStrategy<T>> clone() const override {
        auto clone = std::make_unique<FIFOStrategy<T>>();
        // Клонирование очереди не тривиально, но для целей симуляции
        // можно создать пустую копию
        return clone;
    }
};

// 2. LIFO (Last-In-First-Out) - стек
template<typename T>
class LIFOStrategy : public QueueStrategy<T> {
private:
    std::vector<T> stack_;
    
public:
    void push(const T& item) override {
        stack_.push_back(item);
    }
    
    T pop() override {
        if (stack_.empty()) {
            throw std::runtime_error("Stack is empty");
        }
        T item = stack_.back();
        stack_.pop_back();
        return item;
    }
    
    bool empty() const override {
        return stack_.empty();
    }
    
    size_t size() const override {
        return stack_.size();
    }
    
    std::string name() const override {
        return "LIFO";
    }
    
    std::unique_ptr<QueueStrategy<T>> clone() const override {
        return std::make_unique<LIFOStrategy<T>>();
    }
};

// 3. Random - случайный выбор
template<typename T>
class RandomStrategy : public QueueStrategy<T> {
private:
    std::vector<T> items_;
    mutable std::mt19937 rng_;
    
public:
    RandomStrategy() : rng_(std::random_device{}()) {}
    
    void push(const T& item) override {
        items_.push_back(item);
    }
    
    T pop() override {
        if (items_.empty()) {
            throw std::runtime_error("Queue is empty");
        }
        
        std::uniform_int_distribution<size_t> dist(0, items_.size() - 1);
        size_t idx = dist(rng_);
        T item = items_[idx];
        
        // Удаляем выбранный элемент
        items_[idx] = std::move(items_.back());
        items_.pop_back();
        
        return item;
    }
    
    bool empty() const override {
        return items_.empty();
    }
    
    size_t size() const override {
        return items_.size();
    }
    
    std::string name() const override {
        return "RANDOM";
    }
    
    std::unique_ptr<QueueStrategy<T>> clone() const override {
        return std::make_unique<RandomStrategy<T>>();
    }
};

// 4. Priority - по приоритету (меньший приоритет = выше в очереди)
template<typename T>
class PriorityStrategy : public QueueStrategy<T> {
private:
    struct PriorityItem {
        T item;
        int priority;
        
        bool operator>(const PriorityItem& other) const {
            return priority > other.priority; // Меньший приоритет выше
        }
    };
    
    std::priority_queue<PriorityItem, 
                       std::vector<PriorityItem>,
                       std::greater<PriorityItem>> queue_;
    
public:
    void push(const T& item) override {
        // Для упрощения: приоритет = время прибытия или случайное число
        // В реальности нужен способ определить приоритет
        PriorityItem pitem{item, static_cast<int>(queue_.size())};
        queue_.push(pitem);
    }
    
    T pop() override {
        if (queue_.empty()) {
            throw std::runtime_error("Priority queue is empty");
        }
        T item = queue_.top().item;
        queue_.pop();
        return item;
    }
    
    bool empty() const override {
        return queue_.empty();
    }
    
    size_t size() const override {
        return queue_.size();
    }
    
    std::string name() const override {
        return "PRIORITY";
    }
    
    std::unique_ptr<QueueStrategy<T>> clone() const override {
        return std::make_unique<PriorityStrategy<T>>();
    }
};

// 5. Round Robin - циклическое обслуживание
// Для нескольких ядер - более сложная реализация
template<typename T>
class RoundRobinStrategy : public QueueStrategy<T> {
private:
    std::vector<std::queue<T>> queues_;
    size_t current_queue_;
    
public:
    RoundRobinStrategy(size_t num_queues = 1) 
        : queues_(num_queues), current_queue_(0) {
        if (num_queues == 0) {
            throw std::invalid_argument("Number of queues must be positive");
        }
    }
    
    void push(const T& item) override {
        // Распределяем по очередям циклически
        queues_[current_queue_].push(item);
        current_queue_ = (current_queue_ + 1) % queues_.size();
    }
    
    T pop() override {
        // Ищем первую непустую очередь
        for (size_t i = 0; i < queues_.size(); ++i) {
            size_t idx = (current_queue_ + i) % queues_.size();
            if (!queues_[idx].empty()) {
                T item = queues_[idx].front();
                queues_[idx].pop();
                current_queue_ = (idx + 1) % queues_.size();
                return item;
            }
        }
        throw std::runtime_error("All queues are empty");
    }
    
    bool empty() const override {
        for (const auto& q : queues_) {
            if (!q.empty()) return false;
        }
        return true;
    }
    
    size_t size() const override {
        size_t total = 0;
        for (const auto& q : queues_) {
            total += q.size();
        }
        return total;
    }
    
    std::string name() const override {
        return "ROUND_ROBIN";
    }
    
    std::unique_ptr<QueueStrategy<T>> clone() const override {
        return std::make_unique<RoundRobinStrategy<T>>(queues_.size());
    }
};

// Фабрика для создания стратегий
template<typename T>
class QueueStrategyFactory {
public:
    enum class Type {
        FIFO,
        LIFO,
        RANDOM,
        PRIORITY,
        ROUND_ROBIN
    };
    
    static std::unique_ptr<QueueStrategy<T>> create(Type type, 
                                                   size_t round_robin_queues = 1) {
        switch (type) {
            case Type::FIFO:
                return std::make_unique<FIFOStrategy<T>>();
            case Type::LIFO:
                return std::make_unique<LIFOStrategy<T>>();
            case Type::RANDOM:
                return std::make_unique<RandomStrategy<T>>();
            case Type::PRIORITY:
                return std::make_unique<PriorityStrategy<T>>();
            case Type::ROUND_ROBIN:
                return std::make_unique<RoundRobinStrategy<T>>(round_robin_queues);
            default:
                throw std::invalid_argument("Unknown queue type");
        }
    }
    
    static std::string type_to_string(Type type) {
        switch (type) {
            case Type::FIFO: return "FIFO";
            case Type::LIFO: return "LIFO";
            case Type::RANDOM: return "RANDOM";
            case Type::PRIORITY: return "PRIORITY";
            case Type::ROUND_ROBIN: return "ROUND_ROBIN";
            default: return "UNKNOWN";
        }
    }
    
    static std::vector<Type> get_all_types() {
        return {
            Type::FIFO,
            Type::LIFO,
            Type::RANDOM,
            Type::PRIORITY,
            Type::ROUND_ROBIN
        };
    }
};

} // namespace QueueDisciplines

#endif // QUEUE_DISCIPLINES_H