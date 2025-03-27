#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

using namespace std;

#define err_exit(code, str)                            \
    {                                                  \
        cerr << str << ": " << strerror(code) << endl; \
        exit(EXIT_FAILURE);                            \
    }

#define NUM_PRODUCTS 10

pthread_mutex_t mutex;
int products_count = NUM_PRODUCTS; // Количество продуктов, которые осталось произвести и потребить
bool product_ready = false;        // Флаг готовности продукта
/* Функция, выполняемая потомком-производителем */
void *producer_func(void *arg)
{
    int err;
    while (true)
    {
        // Захватываем мьютекс
        err = pthread_mutex_lock(&mutex);
        if (err != 0)
        {
            err_exit(err, "Cannot lock mutex");
        }
        // Ждем, пока потребитель не потребует товар или не завершено производство
        while (product_ready && products_count > 0)
        {
            // Освобождаем мьютекс, чтобы другой поток мог его захватить
            err = pthread_mutex_unlock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot unlock mutex");
            }
            sleep(1);
            // Захватываем мьютекс
            err = pthread_mutex_lock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot lock mutex");
            }
        }
        // Не допускаем создание товара, если производство завершено
        if (products_count > 0)
        {
            // Производим товар
            cout << "Produced product #" << NUM_PRODUCTS - products_count + 1 << endl;
            product_ready = true;
            // Уменьшаем количество товаров, которые осталось произвести
            products_count--;
        } // Обеспечиваем своевременное завершение потока-производителя
        else if (products_count == 0)
        {
            err = pthread_mutex_unlock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot unlock mutex");
            }
            return NULL;
        }
        // Освобождаем мьютекс
        err = pthread_mutex_unlock(&mutex);
        if (err != 0)
        {
            err_exit(err, "Cannot unlock mutex");
        }
    }
}
/* Функция, выполняемая потомком-потребителем */
void *consumer_func(void *arg)
{
    int err;
    while (true)
    {
        err = pthread_mutex_lock(&mutex);
        if (err != 0)
        {
            err_exit(err, "Cannot lock mutex");
        }
        // Ждем, пока производитель не произведет товар или не завершено производство
        while (!product_ready && products_count > 0)
        {
            // Освобождаем мьютекс, чтобы другой поток мог его захватить
            err = pthread_mutex_unlock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot unlock mutex");
            }
            sleep(1);
            // Захватываем мьютекс
            err = pthread_mutex_lock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot lock mutex");
            }
        }
        // Потребляем товар, если он готов, иначе производство завершено и поток потребителя завершается
        if (product_ready)
        {
            // Потребляем товар
            cout << "Consumed product #" << NUM_PRODUCTS - products_count << endl
                 << endl;
            // Указываем, что товар потреблен
            product_ready = false;
        }
        else
        {
            err = pthread_mutex_unlock(&mutex);
            if (err != 0)
            {
                err_exit(err, "Cannot unlock mutex");
            }
            return NULL;
        }
        // Освобождаем мьютекс
        err = pthread_mutex_unlock(&mutex);
        if (err != 0)
        {
            err_exit(err, "Cannot unlock mutex");
        }
    }
}

int main()
{
    pthread_t thread1, thread2;
    int err;
    // Инициализируем мьютекс
    err = pthread_mutex_init(&mutex, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot initialize mutex");
    }
    // Создаем потоки
    err = pthread_create(&thread1, NULL, producer_func, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot create the producer thread");
    }
    err = pthread_create(&thread2, NULL, consumer_func, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot create the consumer thread");
    }
    // Дожидаемся завершения потоков
    err = pthread_join(thread1, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot join the producer thread");
    }
    err = pthread_join(thread2, NULL);
    if (err != 0)
    {
        err_exit(err, "Cannot join the consumer thread");
    }
    // Освобождаем ресурсы, связанные с мьютексом
    pthread_mutex_destroy(&mutex);
    return 0;
}