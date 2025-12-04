#include "simulator.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <chrono>

using namespace std;

// ==================== КОНСТРУКТОР ====================

Simulator::Simulator(unique_ptr<RandomGenerator> arrival_gen,
                     unique_ptr<RandomGenerator> service_gen,
                     int num_cores,
                     int buffer_cap)
    : current_time_(0.0),
      jobs_completed_(0),
      jobs_lost_(0),
      next_job_id_(0),
      total_arrivals_(0),
      arrival_generator_(std::move(arrival_gen)),
      service_generator_(std::move(service_gen)),
      num_cores_(num_cores),
      buffer_capacity_(buffer_cap),
      total_busy_time_(0.0),
      last_busy_check_time_(0.0)
{
    if (num_cores_ <= 0) {
        throw invalid_argument("Количество ядер должно быть положительным");
    }
    
    cores_busy_.resize(num_cores_, false);
    cores_finish_time_.resize(num_cores_, 0.0);
    cores_current_job_.resize(num_cores_, -1);
}

// ==================== ИНИЦИАЛИЗАЦИЯ ====================

void Simulator::initialize() {
    current_time_ = 0.0;
    jobs_completed_ = 0;
    jobs_lost_ = 0;
    next_job_id_ = 0;
    total_arrivals_ = 0;
    total_busy_time_ = 0.0;
    last_busy_check_time_ = 0.0;
    
    while (!event_queue_.empty()) event_queue_.pop();
    job_queue_.clear();
    active_jobs_.clear();
    
    wait_times_.clear();
    system_times_.clear();
    
    fill(cores_busy_.begin(), cores_busy_.end(), false);
    fill(cores_finish_time_.begin(), cores_finish_time_.end(), 0.0);
    fill(cores_current_job_.begin(), cores_current_job_.end(), -1);
}

// ==================== ОБНОВЛЕНИЕ СТАТИСТИКИ ЗАНЯТОСТИ ====================

void Simulator::update_busy_statistics() {
    if (last_busy_check_time_ < current_time_) {
        double time_since_last_check = current_time_ - last_busy_check_time_;
        int busy_cores = count_busy_cores();
        total_busy_time_ += time_since_last_check * busy_cores;
        last_busy_check_time_ = current_time_;
    }
}

// ==================== ГЛАВНЫЙ ЦИКЛ МОДЕЛИРОВАНИЯ ====================

void Simulator::run(double simulation_time) {
    cout << "Запуск моделирования на время " << simulation_time << " единиц...\n";
    auto start_time = chrono::high_resolution_clock::now();
    
    initialize();
    
    // Планируем первое прибытие
    schedule_next_arrival();
    
    long long iteration_count = 0;
    const long long MAX_ITERATIONS = 100000000;
    
    // Главный цикл событий
    while (!event_queue_.empty() && current_time_ < simulation_time) {
        iteration_count++;
        
        if (iteration_count > MAX_ITERATIONS) {
            cout << "Прервано: достигнуто максимальное количество итераций\n";
            break;
        }
        
        // Извлекаем ближайшее событие
        Event next_event = event_queue_.top();
        event_queue_.pop();
        
        // Обновляем время
        current_time_ = next_event.time;
        
        // Обновляем статистику занятости
        update_busy_statistics();
        
        // Обрабатываем событие
        switch (next_event.type) {
            case Event::ARRIVAL:
                process_arrival();
                break;
            case Event::DEPARTURE:
                process_departure(next_event.job_id, next_event.core_id);
                break;
        }
    }
    
    // Финальный сбор статистики
    update_busy_statistics();
    
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    
    cout << "Моделирование завершено за " << duration.count() << " мс\n";
}

void Simulator::run_until_jobs(int jobs_to_process) {
    cout << "Запуск моделирования до обработки " << jobs_to_process << " заданий...\n";
    auto start_time = chrono::high_resolution_clock::now();
    
    initialize();
    
    // Планируем первое прибытие
    schedule_next_arrival();
    
    long long iteration_count = 0;
    const long long MAX_ITERATIONS = 100000000;
    
    // Главный цикл событий
    while (!event_queue_.empty() && jobs_completed_ < jobs_to_process) {
        iteration_count++;
        
        if (iteration_count > MAX_ITERATIONS) {
            cout << "Прервано: достигнуто максимальное количество итераций\n";
            break;
        }
        
        // Извлекаем ближайшее событие
        Event next_event = event_queue_.top();
        event_queue_.pop();
        
        // Обновляем время
        current_time_ = next_event.time;
        
        // Обновляем статистику занятости
        update_busy_statistics();
        
        // Обрабатываем событие
        switch (next_event.type) {
            case Event::ARRIVAL:
                process_arrival();
                break;
            case Event::DEPARTURE:
                process_departure(next_event.job_id, next_event.core_id);
                break;
        }
    }
    
    // Финальный сбор статистики
    update_busy_statistics();
    
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    
    cout << "Моделирование завершено за " << duration.count() << " мс\n";
}

// ==================== ОБРАБОТКА СОБЫТИЙ ====================

void Simulator::process_arrival() {
    total_arrivals_++;
    
    // Генерируем время обслуживания
    double service_time = service_generator_->generate();
    
    // Создаем задание
    Job new_job(next_job_id_++, current_time_, service_time);
    add_job(new_job);
    
    // Ищем свободное ядро
    int free_core = find_free_core();
    
    if (free_core != -1) {
        // Начинаем обслуживание немедленно
        active_jobs_[new_job.id].start_time = current_time_;
        occupy_core(free_core, new_job.id, current_time_ + service_time);
        schedule_departure(new_job.id, free_core, service_time);
    } else {
        // Все ядра заняты - проверяем буфер
        if (buffer_full()) {
            // Буфер полон - теряем задание
            jobs_lost_++;
            active_jobs_.erase(new_job.id);
        } else {
            // Помещаем в очередь
            job_queue_.push(new_job);
        }
    }
    
    // Планируем следующее прибытие
    schedule_next_arrival();
}

void Simulator::process_departure(int job_id, int core_id) {
    // Находим задание
    auto it = active_jobs_.find(job_id);
    if (it == active_jobs_.end()) {
        cerr << "Ошибка: задание " << job_id << " не найдено\n";
        return;
    }
    
    Job& job = it->second;
    job.finish_time = current_time_;
    
    // Записываем статистику
    record_wait_time(job.wait_time());
    record_system_time(job.system_time());
    
    // Освобождаем ядро
    release_core(core_id);
    
    // Увеличиваем счетчик обработанных заданий
    jobs_completed_++;
    
    // Удаляем задание
    active_jobs_.erase(it);
    
    // Проверяем очередь
    if (!job_queue_.empty()) {
        Job next_job = job_queue_.pop();
        
        // Начинаем обслуживание следующего задания
        auto next_it = active_jobs_.find(next_job.id);
        if (next_it != active_jobs_.end()) {
            Job& job_to_start = next_it->second;
            job_to_start.start_time = current_time_;
            
            double service_time = job_to_start.service_time;
            occupy_core(core_id, job_to_start.id, current_time_ + service_time);
            schedule_departure(job_to_start.id, core_id, service_time);
        }
    }
}

// ==================== ПЛАНИРОВАНИЕ СОБЫТИЙ ====================

void Simulator::schedule_next_arrival() {
    double interval = arrival_generator_->generate();
    double arrival_time = current_time_ + interval;
    
    event_queue_.push(Event(arrival_time, Event::ARRIVAL));
}

void Simulator::schedule_departure(int job_id, int core_id, double service_time) {
    double departure_time = current_time_ + service_time;
    event_queue_.push(Event(departure_time, job_id, core_id));
}

// ==================== РАБОТА С ЯДРАМИ ====================

int Simulator::find_free_core() const {
    for (int i = 0; i < num_cores_; i++) {
        if (!cores_busy_[i]) {
            return i;
        }
    }
    return -1;
}

int Simulator::count_busy_cores() const {
    int count = 0;
    for (bool busy : cores_busy_) {
        if (busy) count++;
    }
    return count;
}

void Simulator::occupy_core(int core_id, int job_id, double finish_time) {
    if (core_id < 0 || core_id >= num_cores_) {
        throw out_of_range("Неверный идентификатор ядра");
    }
    
    cores_busy_[core_id] = true;
    cores_finish_time_[core_id] = finish_time;
    cores_current_job_[core_id] = job_id;
}

void Simulator::release_core(int core_id) {
    if (core_id < 0 || core_id >= num_cores_) {
        throw out_of_range("Неверный идентификатор ядра");
    }
    
    cores_busy_[core_id] = false;
    cores_finish_time_[core_id] = 0.0;
    cores_current_job_[core_id] = -1;
}

// ==================== РАБОТА С ЗАДАНИЯМИ И СТАТИСТИКОЙ ====================

void Simulator::add_job(const Job& job) {
    active_jobs_[job.id] = job;
}

void Simulator::record_wait_time(double time) {
    wait_times_.push_back(time);
}

void Simulator::record_system_time(double time) {
    system_times_.push_back(time);
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================

bool Simulator::buffer_full() const {
    if (buffer_capacity_ == -1) return false;
    return static_cast<int>(job_queue_.size()) >= buffer_capacity_;
}

double Simulator::calculate_rho() const {
    double arrival_intensity = 1.0 / arrival_generator_->mean();
    double service_intensity = 1.0 / service_generator_->mean();
    
    if (service_intensity == 0.0) return 0.0;
    return arrival_intensity / (service_intensity * num_cores_);
}

// ==================== СТАТИСТИЧЕСКИЕ МЕТОДЫ ====================

double Simulator::avg_wait_time() const {
    if (wait_times_.empty()) return 0.0;
    
    double sum = 0.0;
    for (double time : wait_times_) {
        sum += time;
    }
    return sum / wait_times_.size();
}

double Simulator::avg_system_time() const {
    if (system_times_.empty()) return 0.0;
    
    double sum = 0.0;
    for (double time : system_times_) {
        sum += time;
    }
    return sum / system_times_.size();
}

double Simulator::server_utilization() const {
    if (current_time_ == 0.0) return 0.0;
    return total_busy_time_ / (current_time_ * num_cores_);
}

double Simulator::loss_probability() const {
    if (total_arrivals_ == 0) return 0.0;
    return static_cast<double>(jobs_lost_) / total_arrivals_;
}

double Simulator::avg_queue_length() const {
    // Упрощенный расчет: среднее количество заданий в системе
    // Для точного расчета нужно было бы делать временные семплы
    if (current_time_ == 0.0) return 0.0;
    
    // Приблизительная оценка по формуле Литтла: L = λW
    double arrival_intensity = 1.0 / arrival_generator_->mean();
    return arrival_intensity * avg_wait_time();
}

double Simulator::avg_busy_cores() const {
    if (current_time_ == 0.0) return 0.0;
    return total_busy_time_ / current_time_;
}

double Simulator::min_wait_time() const {
    if (wait_times_.empty()) return 0.0;
    return *min_element(wait_times_.begin(), wait_times_.end());
}

double Simulator::max_wait_time() const {
    if (wait_times_.empty()) return 0.0;
    return *max_element(wait_times_.begin(), wait_times_.end());
}

double Simulator::wait_time_variance() const {
    if (wait_times_.size() < 2) return 0.0;
    
    double mean = avg_wait_time();
    double sum_sq = 0.0;
    
    for (double time : wait_times_) {
        double diff = time - mean;
        sum_sq += diff * diff;
    }
    
    return sum_sq / (wait_times_.size() - 1);
}

double Simulator::system_time_variance() const {
    if (system_times_.size() < 2) return 0.0;
    
    double mean = avg_system_time();
    double sum_sq = 0.0;
    
    for (double time : system_times_) {
        double diff = time - mean;
        sum_sq += diff * diff;
    }
    
    return sum_sq / (system_times_.size() - 1);
}

// ==================== МЕТОДЫ ВЫВОДА ====================

void Simulator::print_configuration() const {
    cout << fixed << setprecision(4);
    cout << "\nКОНФИГУРАЦИЯ СИМУЛЯТОРА:\n";
    cout << "  Распределение прибытий: " << arrival_generator_->name() << "\n";
    cout << "  Распределение обслуживания: " << service_generator_->name() << "\n";
    cout << "  Количество ядер: " << num_cores_ << "\n";
    cout << "  Ёмкость буфера: " << (buffer_capacity_ == -1 ? "∞" : to_string(buffer_capacity_)) << "\n";
    cout << "  Дисциплина очереди: FIFO\n";
    
    double rho_value = calculate_rho();
    cout << "  Загрузка системы ρ: " << rho_value;
    if (rho_value < 1.0) {
        cout << " (система стационарна)";
    } else {
        cout << " (система НЕстационарна!)";
    }
    cout << "\n";
}

void Simulator::print_statistics() const {
    cout << "\n========== РЕЗУЛЬТАТЫ МОДЕЛИРОВАНИЯ ==========\n\n";
    
    cout << "ОСНОВНЫЕ ПОКАЗАТЕЛИ:\n";
    cout << fixed << setprecision(2);
    cout << "  Время моделирования: " << current_time_ << "\n";
    cout << "  Всего поступило заданий: " << total_arrivals_ << "\n";
    cout << "  Обработано заданий: " << jobs_completed_ << "\n";
    cout << "  Потеряно заданий: " << jobs_lost_ << "\n";
    cout << "  Текущая длина очереди: " << job_queue_.size() << "\n\n";
    
    cout << "СТАТИСТИКА СИСТЕМЫ:\n";
    cout << fixed << setprecision(4);
    cout << "  Среднее время ожидания W: " << avg_wait_time() << "\n";
    cout << "  Среднее время в системе U: " << avg_system_time() << "\n";
    cout << "  Загрузка сервера: " << server_utilization() * 100 << "%\n";
    cout << "  Вероятность потери: " << loss_probability() * 100 << "%\n";
    cout << "  Среднее занятых ядер: " << avg_busy_cores() << " из " << num_cores_ << "\n";
    
    double arrival_intensity = 1.0 / arrival_generator_->mean();
    double lambdaW = arrival_intensity * avg_wait_time();
    double L_approx = avg_queue_length(); // Упрощенная оценка
    
    cout << "\nПРОВЕРКА ФОРМУЛЫ ЛИТТЛА (L ≈ λW):\n";
    cout << "  λW = " << arrival_intensity << " × " << avg_wait_time() << " = " << lambdaW << "\n";
    cout << "  L (приблизительно) = " << L_approx << "\n";
    cout << "  Отклонение: " << abs(L_approx - lambdaW) / max(L_approx, 0.001) * 100 << "%\n";
    
    if (num_cores_ == 1 && calculate_rho() < 1.0) {
        double rho = calculate_rho();
        double mu = 1.0 / service_generator_->mean();
        
        double theoretical_wait = rho / (mu * (1 - rho));
        double theoretical_queue = (rho * rho) / (1 - rho);
        
        cout << "\nСРАВНЕНИЕ С ТЕОРИЕЙ (M/M/1):\n";
        cout << "  Теор. среднее время ожидания: " << theoretical_wait;
        cout << " (отклонение: " << abs(avg_wait_time() - theoretical_wait) / theoretical_wait * 100 << "%)\n";
        cout << "  Теор. средняя длина очереди: " << theoretical_queue;
        cout << " (отклонение: " << abs(L_approx - theoretical_queue) / theoretical_queue * 100 << "%)\n";
    }
}

void Simulator::save_statistics(const std::string& filename) const {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Ошибка: не удалось открыть файл " << filename << "\n";
        return;
    }
    
    file << fixed << setprecision(6);
    file << "parameter,value\n";
    file << "simulation_time," << current_time_ << "\n";
    file << "total_arrivals," << total_arrivals_ << "\n";
    file << "jobs_completed," << jobs_completed_ << "\n";
    file << "jobs_lost," << jobs_lost_ << "\n";
    file << "avg_wait_time," << avg_wait_time() << "\n";
    file << "avg_system_time," << avg_system_time() << "\n";
    file << "server_utilization," << server_utilization() << "\n";
    file << "loss_probability," << loss_probability() << "\n";
    file << "arrival_intensity," << 1.0/arrival_generator_->mean() << "\n";
    file << "service_intensity," << 1.0/service_generator_->mean() << "\n";
    file << "rho," << calculate_rho() << "\n";
    
    file.close();
    cout << "Статистика сохранена в " << filename << "\n";
}