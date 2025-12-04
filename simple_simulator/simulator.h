#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "random_generator.h"
#include <queue>
#include <memory>
#include <vector>
#include <map>
#include <string>

// ==================== ОСНОВНЫЕ СТРУКТУРЫ ДАННЫХ ====================

/**
 * Задание, поступающее в систему
 */
struct Job {
    int id;                    // уникальный идентификатор
    double arrival_time;       // время поступления
    double service_time;       // требуемое время обслуживания
    double start_time;         // время начала обслуживания
    double finish_time;        // время завершения обслуживания
    
    Job() : id(-1), arrival_time(0), service_time(0), start_time(-1), finish_time(-1) {}
    
    Job(int _id, double arrival, double service)
        : id(_id), arrival_time(arrival), service_time(service),
          start_time(-1.0), finish_time(-1.0) {}
    
    double wait_time() const {
        return (start_time >= 0) ? start_time - arrival_time : 0.0;
    }
    
    double system_time() const {
        return (finish_time >= 0) ? finish_time - arrival_time : 0.0;
    }
};

/**
 * Событие в системе массового обслуживания
 */
struct Event {
    enum Type { ARRIVAL, DEPARTURE };
    
    double time;        // время события
    Type type;          // тип события
    int job_id;         // идентификатор задания (для DEPARTURE)
    int core_id;        // идентификатор ядра (для DEPARTURE)
    
    Event(double t, Type tp) : time(t), type(tp), job_id(-1), core_id(-1) {}
    Event(double t, int jid, int cid) : time(t), type(DEPARTURE), job_id(jid), core_id(cid) {}
    
    // Для приоритетной очереди (раньше время → выше приоритет)
    bool operator>(const Event& other) const {
        return time > other.time;
    }
};

/**
 * Очередь FIFO для заданий
 */
class FIFOQueue {
private:
    std::queue<Job> queue_;
    
public:
    void push(const Job& job) { 
        queue_.push(job); 
    }
    
    Job pop() { 
        if (queue_.empty()) {
            throw std::runtime_error("Попытка извлечения из пустой очереди");
        }
        Job job = queue_.front(); 
        queue_.pop(); 
        return job; 
    }
    
    bool empty() const { 
        return queue_.empty(); 
    }
    
    size_t size() const { 
        return queue_.size(); 
    }
    
    void clear() { 
        while (!queue_.empty()) queue_.pop(); 
    }
};

// ==================== КЛАСС СИМУЛЯТОРА ====================

/**
 * Событийно-ориентированный симулятор системы массового обслуживания
 */
class Simulator {
private:
    // Системные переменные
    double current_time_;          // текущее системное время
    int jobs_completed_;           // количество обработанных заданий
    int jobs_lost_;                // количество потерянных заданий
    int next_job_id_;              // следующий ID задания
    int total_arrivals_;           // всего поступило заданий
    
    // Конфигурация системы
    std::unique_ptr<RandomGenerator> arrival_generator_;
    std::unique_ptr<RandomGenerator> service_generator_;
    int num_cores_;                // количество ядер сервера
    int buffer_capacity_;          // ёмкость буфера (-1 = бесконечный)
    
    // Состояние системы
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> event_queue_;
    FIFOQueue job_queue_;
    
    std::vector<bool> cores_busy_;           // занятость ядер
    std::vector<double> cores_finish_time_;  // время завершения на ядрах
    std::vector<int> cores_current_job_;     // текущие задания на ядрах
    
    std::map<int, Job> active_jobs_;         // активные задания
    
    // Статистика
    std::vector<double> wait_times_;         // времена ожидания
    std::vector<double> system_times_;       // времена пребывания в системе
    double total_busy_time_;                 // суммарное время занятости ядер
    double last_busy_check_time_;            // последняя проверка занятости
    
    // Приватные методы
    void initialize();
    void process_arrival();
    void process_departure(int job_id, int core_id);
    
    void schedule_next_arrival();
    void schedule_departure(int job_id, int core_id, double service_time);
    
    int find_free_core() const;
    void occupy_core(int core_id, int job_id, double finish_time);
    void release_core(int core_id);
    int count_busy_cores() const;
    void update_busy_statistics();
    
    void add_job(const Job& job);
    void record_wait_time(double time);
    void record_system_time(double time);
    
    bool buffer_full() const;
    double calculate_rho() const;
    
public:
    /**
     * Конструктор симулятора
     * @param arrival_gen генератор интервалов между прибытиями
     * @param service_gen генератор времени обслуживания
     * @param num_cores количество ядер сервера
     * @param buffer_cap ёмкость буфера (-1 = бесконечный)
     */
    Simulator(std::unique_ptr<RandomGenerator> arrival_gen,
              std::unique_ptr<RandomGenerator> service_gen,
              int num_cores = 1,
              int buffer_cap = -1);
    
    // Запрет копирования
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    
    /**
     * Запуск моделирования
     * @param simulation_time время моделирования
     */
    void run(double simulation_time);
    
    /**
     * Запуск моделирования до обработки заданного количества заданий
     * @param jobs_to_process количество заданий для обработки
     */
    void run_until_jobs(int jobs_to_process);
    
    // ============= СТАТИСТИЧЕСКИЕ МЕТОДЫ =============
    
    double avg_wait_time() const;
    double avg_system_time() const;
    double server_utilization() const;
    double loss_probability() const;
    double avg_queue_length() const;
    double avg_busy_cores() const;
    
    double min_wait_time() const;
    double max_wait_time() const;
    double wait_time_variance() const;
    double system_time_variance() const;
    
    double rho() const { return calculate_rho(); }
    bool is_stationary() const { return calculate_rho() < 1.0; }
    
    // ============= МЕТОДЫ ВЫВОДА =============
    
    void print_configuration() const;
    void print_statistics() const;
    void save_statistics(const std::string& filename) const;
    
    // ============= GETTERS =============
    
    double current_time() const { return current_time_; }
    int jobs_completed() const { return jobs_completed_; }
    int jobs_lost() const { return jobs_lost_; }
    int total_arrivals() const { return total_arrivals_; }
    int jobs_in_system() const { return active_jobs_.size(); }
    int queue_length() const { return job_queue_.size(); }
    bool is_server_busy() const { return count_busy_cores() > 0; }
};

#endif // SIMULATOR_H