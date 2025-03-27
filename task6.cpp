#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <map>
#include <string>
#define err_exit(code, str)                            \
    {                                                  \
        cerr << str << ": " << strerror(code) << endl; \
        exit(EXIT_FAILURE);                            \
    }

using namespace std;
using namespace std::chrono;
const int DEFAULT_NUM_THREADS = 4;                                                                                                                                                                         // Количество потоков по умолчанию
const int DEFAULT_NUM_DUPLICATES_STR = 2;                                                                                                                                                                  // Количество дубликатов строки по умолчанию
const string LETTERS = "aeiouy";                                                                                                                                                                           // Набор символов для подсчета
const string DEFAULT_LINE = "C++ pthread is a POSIX (Portable Operating System Interface) library used for creating and managing threads in C++ applications, allowing for concurrent execution of code."; // Строка для подсчета символов по умолчанию
pthread_mutex_t mutex;
struct MapThreadArgs
{
    vector<string> *lines;               // указатель на вкетор строк для обработки
    int begin;                           // Начальный индекс диапазона строк, который обрабатывает поток
    int end;                             // Конечный индекс диапазона строк, который обрабатывает поток
    vector<map<char, int>> *map_results; // Указатель на словарь, с результатами работы функции map
};
struct ReduceThreadArgs
{
    vector<map<char, int>> *map_results; // Указатель на словарь, с результатми работы map
    int begin;                           // Начальный индекс диапазона строк, который обрабатывает поток
    int end;                             // Конечный индекс диапазона строк, который обрабатывает поток
    map<char, int> *reduce_results;      // Указатель на словарь, с результатми работы функции reduce
};

/* Функция для подсчета количества вхождения набора символов для сегмента строк */
void *map_func(void *arg)
{
    MapThreadArgs *args = static_cast<MapThreadArgs *>(arg);
    // Обрабатываем каждую строку из диапазона [begin, end]
    for (int i = args->begin; i <= args->end; i++)
    {
        // Проходим по каждому символу в строке
        for (char letter : (*args->lines)[i])
        {
            // Приводим символ к нижнему регистру
            char letter_lower = tolower(letter);
            // Если символ входит в набор символов для подсчета
            if (LETTERS.find(letter_lower) != string::npos)
            {
                // Увеличиваем счетчик для данного символа в текущем словаре
                (*args->map_results)[i][letter_lower]++;
            }
        }
    }
    return nullptr;
}

/* Функция для суммированния результатов подсчета количесвта вхождений набора
символов для сегментов строк */
void *reduce_func(void *arg)
{
    ReduceThreadArgs *args = static_cast<ReduceThreadArgs *>(arg);
    int err;
    // Проходим по заданному диапазону результатов
    for (int i = args->begin; i <= args->end; i++)
    {
        // Для каждой пары (символ, количество вхождений) из текущего словаря
        for (const auto &pair : (*args->map_results)[i])
        {
            // Захватываем мьютекс
            err = pthread_mutex_lock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot lock mutex");
            }
            // Добавляем значение из текущего словаря в общий результат
            (*args->reduce_results)[pair.first] += pair.second;
            // Освобождаем мьютекс
            err = pthread_mutex_unlock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot unlock mutex");
            }
        }
    }
    return nullptr;
}
/*
Функция реализующая модель MapReduce. Распределяет работу между
несколькими потоками для функций map и reduce
*/
map<char, int> map_reduce(
    vector<string> &lines,
    void *(*map_thread_func)(void *),
    void *(*reduce_thread_func)(void *),
    int num_threads)
{
    // Если количество потоков больше количества строк, ограничиваем число потоков числом строк
    int map_num_threads = num_threads > lines.size() ? lines.size() : num_threads;
    // Инициализируем вектор для хранения промежуточных результатов: на одну строку по одному словарю
    vector<map<char, int>>
        map_results(lines.size());
    vector<pthread_t> map_threads(map_num_threads);         // Вектор идентификаторов map-потоков
    vector<MapThreadArgs> map_thread_args(map_num_threads); // Вектор параметров для каждого map - потока
    int err;
    // Распределяем строки между потоками
    int base_segment_size = lines.size() / map_num_threads;
    int remainder = lines.size() % map_num_threads;
    // Создаем потоки, каждый из которых обрабатывает свой сегмент строк
    for (int i = 0; i < map_num_threads; ++i)
    {
        int begin = i * base_segment_size + min(i, remainder);
        int end = begin + base_segment_size - (i < remainder ? 0 : 1);
        map_thread_args[i] = {&lines, begin, end, &map_results};
        err = pthread_create(&map_threads[i], nullptr, map_thread_func, &map_thread_args[i]);
        if (err != 0)
        {
            err_exit(err, "Cannot create the map thread");
        }
    }

    // Ожидаем завершения работы всех map-потоков
    for (auto &thread : map_threads)
    {
        err = pthread_join(thread, nullptr);
        if (err != 0)
        {
            err_exit(err, "Cannot join a map thread");
        }
    }
    // Если число потоков больше, чем элементов map_results, ограничиваем число потоков числом элементов map_results
    int reduce_num_threads = num_threads > map_results.size() ? map_results.size() : num_threads;
    vector<pthread_t> reduce_threads(reduce_num_threads);            // Вектор идентификаторов reduce - потоков
    vector<ReduceThreadArgs> reduce_thread_args(reduce_num_threads); // Вектор параметров для каждого reduce - потока
    // Инициализация словар для хранения итогового результата
    map<char, int> reduce_results;

    for (char letter : LETTERS)
    {
        reduce_results[letter] = 0;
    }
    // Распределяем строки между потоками
    base_segment_size = map_results.size() / reduce_num_threads;
    remainder = map_results.size() % reduce_num_threads;
    for (int i = 0; i < reduce_num_threads; ++i)
    {
        int begin = i * base_segment_size + min(i, remainder);
        int end = begin + base_segment_size - (i < remainder ? 0 : 1);
        reduce_thread_args[i] = {&map_results, begin, end, &reduce_results};
        err = pthread_create(&reduce_threads[i], nullptr, reduce_thread_func,
                             &reduce_thread_args[i]);
        if (err != 0)
        {
            err_exit(err, "Cannot create the reduce thread");
        }
    }
    // Ожидаем завершения работы всех reduce-потоков
    for (auto &thread : reduce_threads)
    {
        err = pthread_join(thread, nullptr);
        if (err != 0)
        {
            err_exit(err, "Cannot join a map thread");
        }
    }
    return reduce_results;
}
int main()
{
    char repeat;
    int err;
    do
    {
        // Ввод параметров
        int num_threads, num_duplicates;
        cout << "Enter number of threads: ";
        cin >> num_threads;
        cout << "Enter number of duplicates: ";
        cin >> num_duplicates;
        // Инициализация данных
        vector<string> lines(num_duplicates, DEFAULT_LINE);
        // Инициализация мьютекса
        err = pthread_mutex_init(&mutex, nullptr);
        if (err != 0)
            err_exit(err, "Cannot initialize mutex");
        // Цикл замеров времени
        double min_time = numeric_limits<double>::max();
        map<char, int> best_result;
        for (int i = 0; i < 100; ++i)
        {
            auto start = high_resolution_clock::now();
            auto result = map_reduce(lines, map_func, reduce_func, num_threads);
            auto end = high_resolution_clock::now();
            double current_time = duration<double>(end - start).count();
            if (current_time < min_time)
            {
                min_time = current_time;
                best_result = result;
            }
        }
        // Вывод результатов
        cout << "\nBest execution time: " << min_time << "s\n";
        cout << "Letter counts:\n";
        for (const auto &pair : best_result)
        {
            cout << pair.first << ": " << pair.second << endl;
        }
        // Очистка ресурсов
        pthread_mutex_destroy(&mutex);
        // Запрос на повторение
        cout << "\nRepeat test? (y/n): ";
        cin >> repeat;
    } while (repeat == 'y' || repeat == 'Y');
    return 0;
}