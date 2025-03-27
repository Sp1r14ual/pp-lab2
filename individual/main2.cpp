#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <stdio.h>
#include <fstream>
#include <string>
#include <queue>
#include <pthread.h>

#define err_exit(code, str)                                              \
        {                                                                \
                std::cerr << str << ": " << strerror(code) << std::endl; \
                exit(EXIT_FAILURE);                                      \
        }

struct thread_attr
{
        const char *substring;
};

std::queue<const char *> tasks_queue;
pthread_mutex_t mutex;
bool isNoMoreTasksComing = false;
void (*do_task_func)(void *, const char *);

void *thread_pool_function(void *arg)
{
        int err;

        while (true)
        {
                err = pthread_mutex_lock(&mutex);
                if (err != 0)
                        err_exit(err, "Cannot lock mutex");

                if (tasks_queue.empty())
                {
                        err = pthread_mutex_unlock(&mutex);
                        if (err != 0)
                                err_exit(err, "Cannot unlock mutex");

                        if (isNoMoreTasksComing)
                                return NULL;

                        continue;
                }

                const char *task = tasks_queue.front();
                tasks_queue.pop();

                err = pthread_mutex_unlock(&mutex);
                if (err != 0)
                        err_exit(err, "Cannot unlock mutex");

                do_task_func(arg, task);

                if (err != 0)
                        err_exit(err, "Cannot unlock mutex");
        }
}

void add_task(const char *new_task)
{
        int err;

        if (isNoMoreTasksComing)
                return;

        err = pthread_mutex_lock(&mutex);
        if (err != 0)
                err_exit(err, "Cannot lock mutex");

        char *task_copy = strdup(new_task);
        tasks_queue.push(task_copy);

        err = pthread_mutex_unlock(&mutex);
        if (err != 0)
                err_exit(err, "Cannot unlock mutex");
}

void stop_receiving_tasks()
{
        int err;

        err = pthread_mutex_lock(&mutex);
        if (err != 0)
                err_exit(err, "Cannot lock mutex");

        isNoMoreTasksComing = true;

        err = pthread_mutex_unlock(&mutex);
        if (err != 0)
                err_exit(err, "Cannot unlock mutex");
}

void find_substring_in_line_recursively(const char *substring, uint substr_len, std::string line, const char *file_path, uint line_index, int line_pos)
{
        size_t pos = line.find(substring);

        if (pos == std::string::npos)
        {
                return;
        }

        std::cout << "Find substring in file " << file_path << " in line №" << line_index << " on position " << line_pos + pos << "-" << line_pos + pos + substr_len - 1 << std::endl;

        find_substring_in_line_recursively(substring, substr_len, line.substr(pos + 1), file_path, line_index, line_pos + pos + 1);
}

void find_substrings_in_file(void *arg, const char *file_path)
{
        thread_attr *thread_struct = (thread_attr *)arg;

        std::ifstream file(file_path);
        if (!file)
        {
                std::cout << "Cannot read file " << file_path << std::endl;
                return;
        }

        std::string line;
        uint line_index = 1;

        while (std::getline(file, line))
        {
                find_substring_in_line_recursively(thread_struct->substring, std::strlen(thread_struct->substring), line, file_path, line_index, 0);
                line_index++;
        }

        file.close();
}

void find_substring_recursively(const char *substring, const char *directory)
{
        DIR *dir = opendir(directory);
        if (!dir)
        {
                std::cout << "Cannot open a directory" << std::endl;
                return;
        }

        struct stat info;
        struct dirent *files;
        char temp_path[PATH_MAX];
        size_t file_name_length;

        while ((files = readdir(dir)) != NULL)
        {
                if (strcmp(files->d_name, ".") == 0 || strcmp(files->d_name, "..") == 0)
                        continue;

                file_name_length = std::strlen(files->d_name);
                snprintf(temp_path, PATH_MAX, "%s/%s", directory, files->d_name);

                if (stat(temp_path, &info) == 0 && S_ISDIR(info.st_mode))
                {
                        // std::cout << "dir: " << temp_path << std::endl;
                        find_substring_recursively(substring, temp_path);
                }

                else if (file_name_length > 4 && strcmp(files->d_name + file_name_length - 4, ".txt") == 0)
                {
                        // std::cout << "txt: " << temp_path << std::endl;
                        add_task(temp_path);
                }
        }

        closedir(dir);
}

void find_substring_in_all_files(const char *substring, const char *directory)
{
        find_substring_recursively(substring, directory);

        stop_receiving_tasks();
}

int main(int argc, char *argv[])
{
        if (argc != 4)
        {
                std::cout << "substring? threads_num? directory?" << std::endl;
                exit(-1);
        }

        const char *substring = argv[1];

        int threads_num = atoi(argv[2]);
        if (threads_num < 1)
        {
                std::cout << "threads_num < 1" << std::endl;
                return -1;
        }

        const char *directory = argv[3];
        if (strcmp(directory, "PWD") == 0)
        {
                char path[PATH_MAX];
                getcwd(path, PATH_MAX);
                directory = path;
        }
        else
        {
                struct stat info;
                if (!(stat(directory, &info) == 0 && S_ISDIR(info.st_mode)))
                {
                        std::cout << directory << " not a directory" << std::endl;
                        return -1;
                }
        }

        thread_attr arg;
        arg.substring = substring;

        do_task_func = find_substrings_in_file;

        int err;
        pthread_t *threads;
        threads = new pthread_t[threads_num];

        for (int i = 0; i < threads_num; i++)
        {
                err = pthread_create(&threads[i], NULL, thread_pool_function, &arg);
                if (err != 0)
                        err_exit(err, "Cannot create a thread");
        }

        err = pthread_mutex_init(&mutex, NULL);
        if (err != 0)
                err_exit(err, "Cannot initialize mutex");

        find_substring_in_all_files(substring, directory);

        for (int i = 0; i < threads_num; i++)
        {
                pthread_join(threads[i], NULL);
                if (err != 0)
                        err_exit(err, "Cannot join a thread");
        }

        pthread_mutex_destroy(&mutex);

        pthread_exit(NULL);

        return 0;
}
