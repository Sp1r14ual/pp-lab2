#include <iostream>
#include <pthread.h>
#include <vector>
#include <chrono>
#include <limits>

using namespace std;
using namespace std::chrono;

struct ThreadArgs
{
    int *counter;    // Указатель на общий счетчик
    int count_tasks; // Количество инкрементов на поток
};

pthread_mutex_t mutex;
pthread_spinlock_t spinlock;

/* Функция потока с использованием мьютекса */
void *mutex_increment(void *arg)
{
    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    for (int i = 0; i < args->count_tasks; ++i)
    {
        pthread_mutex_lock(&mutex);
        // Захват мьютекса
        (*(args->counter))++;
        // Критическая секция
        pthread_mutex_unlock(&mutex);
        // Освобождение мьютекса
    }
    return nullptr;
}

/* Функция потока с использованием спинлока */
void *spinlock_increment(void *arg)
{
    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    for (int i = 0; i < args->count_tasks; ++i)
    {
        pthread_spin_lock(&spinlock);   // Захват спинлока
        (*(args->counter))++;           // Критическая секция
        pthread_spin_unlock(&spinlock); // Освобождение спинлока
    }
    return nullptr;
}

/* Запуск теста с мьютексом */
double run_mutex_test(int count_threads, int count_tasks, int &final_counter)
{
    final_counter = 0; // Сброс счетчика перед тестом
    vector<pthread_t> threads(count_threads);
    ThreadArgs args = {&final_counter, count_tasks};
    auto start = high_resolution_clock::now(); // Старт замера
    // Создание потоков
    for (int i = 0; i < count_threads; ++i)
    {
        if (pthread_create(&threads[i], nullptr, mutex_increment, &args) != 0)
        {
            cerr << "Error creating thread (mutex test)." << endl;
            exit(EXIT_FAILURE);
        }
    }
    // Ожидание завершения всех потоков
    for (int i = 0; i < count_threads; ++i)
    {
        pthread_join(threads[i], nullptr);
    }
    auto end = high_resolution_clock::now(); // Финиш замера
    return duration<double>(end - start).count();
}
/* Запуск теста со спинлоком */
double run_spinlock_test(int count_threads, int count_tasks, int &final_counter)
{
    final_counter = 0;
    vector<pthread_t> threads(count_threads);
    ThreadArgs args = {&final_counter, count_tasks};
    auto start = high_resolution_clock::now();
    for (int i = 0; i < count_threads; ++i)
    {
        if (pthread_create(&threads[i], nullptr, spinlock_increment, &args) != 0)
        {
            cerr << "Error creating thread (spinlock test)." << endl;
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < count_threads; ++i)
    {
        pthread_join(threads[i], nullptr);
    }
    auto end = high_resolution_clock::now();
    return duration<double>(end - start).count();
}
int main()
{
    int count_threads, count_tasks;
    char repeat;
    // Инициализация примитивов синхронизации
    pthread_mutex_init(&mutex, nullptr);
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
    do
    {
        cout << "Enter count of threads: ";
        cin >> count_threads;
        cout << "Enter count of tasks per thread: ";
        cin >> count_tasks;
        // Тестирование мьютекса (100 замеров)
        double min_mutex_time = numeric_limits<double>::max();
        int mutex_counter = 0;
        for (int i = 0; i < 100; ++i)
        {
            int current_counter;
            double current_time = run_mutex_test(count_threads, count_tasks, current_counter);

            // Сохраняем минимальное время и соответствующий счетчик
            if (current_time < min_mutex_time)
            {
                min_mutex_time = current_time;
                mutex_counter = current_counter;
            }
        }
        // Тестирование спинлока (100 замеров)
        double min_spinlock_time = numeric_limits<double>::max();
        int spinlock_counter = 0;
        for (int i = 0; i < 100; ++i)
        {
            int current_counter;
            double current_time = run_spinlock_test(count_threads, count_tasks, current_counter);
            if (current_time < min_spinlock_time)
            {
                min_spinlock_time = current_time;
                spinlock_counter = current_counter;
            }
        }

        cout << fixed;
        // Вывод результатов
        cout << "Mutex test result: min time = " << min_mutex_time
             << " seconds, final counter = " << mutex_counter << endl;
        cout << "Spinlock test result: min time = " << min_spinlock_time
             << " seconds, final counter = " << spinlock_counter << endl;
        cout << "Run test again? (y/n): ";
        cin >> repeat;
    } while (repeat == 'y' || repeat == 'Y');
    // Освобождение ресурсов примитивов
    pthread_mutex_destroy(&mutex);
    pthread_spin_destroy(&spinlock);
    return 0;
}