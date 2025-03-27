#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <sstream>
#include <unistd.h>
#include <cmath>

using namespace std;

#define err_exit(code, str)                            \
    {                                                  \
        cerr << str << ": " << strerror(code) << endl; \
        exit(EXIT_FAILURE);                            \
    }

const int TASKS_COUNT = 10;
int task_list[TASKS_COUNT];
// Массив заданий
int current_task = 0; // Указатель на текущее задание
// Мьютекс
pthread_mutex_t mutex;
// Функция, выполняющая продолжительную операцию
void do_task(int task_no)
{
    double result = 0;
    for (int i = 1; i < 10000; i++)
    {
        result += exp(log(i));
    }
}
// Функция, выполняемая потоком
void *thread_job(void *arg)
{
    int task_no;
    int err; // Перебираем в цикле доступные задания
    while (true)
    {
        sleep(rand() % 2 + 1);
        // Захватываем мьютекс для исключительного доступа
        // к указателю текущего задания (переменная current_task)
        // err = pthread_mutex_lock(&mutex);
        if (err != 0)
        {
            err_exit(err, "Cannot lock mutex");
        }
        // Запоминаем номер текущего задания, которое будем исполнять
        task_no = current_task;
        // Сдвигаем указатель текущего задания на следующее
        current_task++;
        // Освобождаем мьютекс
        // err = pthread_mutex_unlock(&mutex);
        if (err != 0)
        {
            err_exit(err, "Cannot unlock mutex");
        }
        // Если запомненный номер задания не превышает
        // количества заданий, вызываем функцию, которая
        // выполнит задание.
        // В противном случае завершаем работу потока
        if (task_no < TASKS_COUNT)
        {
            do_task(task_no);
            cout << pthread_self() << "\t\t\t" << task_no << "\n";
        }
        else
        {
            return NULL;
        }
    }
}
int main()
{
    srand(time(0));
    // Идентификаторы потоков
    pthread_t thread1;
    pthread_t thread2;
    int err; // Код ошибки
    // Инициализируем массив заданий случайными числами
    for (int i = 0; i < TASKS_COUNT; ++i)
    {
        task_list[i] = rand() % TASKS_COUNT;
    }
    cout << "thread_id\t\t\ttask_no" << endl;
    // Инициализируем мьютекс
    err = pthread_mutex_init(&mutex, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot initialize mutex");
    }
    // Создаем потоки
    err = pthread_create(&thread1, NULL, thread_job, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot create thread 1");
    }
    err = pthread_create(&thread2, NULL, thread_job, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot create thread 2");
    }
    err = pthread_join(thread1, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot join a thread 1");
    }
    err = pthread_join(thread2, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot join a thread 2");
    }
    // Освобождаем ресурсы, связанные с мьютексом
    pthread_mutex_destroy(&mutex);
    return 0;
}