#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <limits.h>

#define MAX_THREADS 4

typedef struct task_node
{
    char *path;
    int is_dir;
    struct task_node *next;
} task_node;

typedef struct
{
    task_node *head;
    task_node *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int active_threads;
    int shutdown;
} task_queue;

typedef struct
{
    task_queue *queue;
    const char *substring;
    pthread_mutex_t output_mutex;
} thread_data;

void queue_init(task_queue *q)
{
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->active_threads = 0;
    q->shutdown = 0;
}

void enqueue(task_queue *q, const char *path, int is_dir)
{
    task_node *new_node = (task_node *)malloc(sizeof(task_node));
    new_node->path = strdup(path);
    new_node->is_dir = is_dir;
    new_node->next = NULL;

    pthread_mutex_lock(&q->mutex);
    if (q->tail == NULL)
    {
        q->head = q->tail = new_node;
    }
    else
    {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

task_node *dequeue(task_queue *q)
{
    pthread_mutex_lock(&q->mutex);
    while (q->head == NULL && !q->shutdown)
    {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    if (q->shutdown)
    {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    task_node *node = q->head;
    q->head = node->next;
    if (q->head == NULL)
    {
        q->tail = NULL;
    }
    q->active_threads++;
    pthread_mutex_unlock(&q->mutex);
    return node;
}

void task_complete(task_queue *q)
{
    pthread_mutex_lock(&q->mutex);
    q->active_threads--;
    if (q->head == NULL && q->active_threads == 0)
    {
        q->shutdown = 1;
        pthread_cond_broadcast(&q->cond);
    }
    pthread_mutex_unlock(&q->mutex);
}

void process_file(const char *path, const char *substring, pthread_mutex_t *output_mutex)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return;

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(fsize + 1);
    if (!content)
    {
        fclose(file);
        return;
    }

    fread(content, 1, fsize, file);
    content[fsize] = 0;

    if (strstr(content, substring))
    {
        pthread_mutex_lock(output_mutex);
        printf("Found '%s' in: %s\n", substring, path);
        pthread_mutex_unlock(output_mutex);
    }

    free(content);
    fclose(file);
}

void *worker(void *arg)
{
    thread_data *data = (thread_data *)arg;
    task_queue *q = data->queue;

    while (1)
    {
        task_node *node = dequeue(q);
        if (!node)
            break;

        if (node->is_dir)
        {
            DIR *dir = opendir(node->path);
            if (dir)
            {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL)
                {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                    char full_path[PATH_MAX];
                    snprintf(full_path, sizeof(full_path), "%s/%s", node->path, entry->d_name);

                    struct stat st;
                    if (stat(full_path, &st) != 0)
                        continue;

                    enqueue(q, full_path, S_ISDIR(st.st_mode));
                }
                closedir(dir);
            }
        }
        else
        {
            const char *ext = strrchr(node->path, '.');
            if (ext && strcmp(ext, ".txt") == 0)
            {
                process_file(node->path, data->substring, &data->output_mutex);
            }
        }

        free(node->path);
        free(node);
        task_complete(q);
    }
    return NULL;
}

void queue_destroy(task_queue *q)
{
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
    task_node *current = q->head;
    while (current)
    {
        task_node *next = current->next;
        free(current->path);
        free(current);
        current = next;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <directory> <substring>\n", argv[0]);
        return 1;
    }

    task_queue q;
    queue_init(&q);
    enqueue(&q, argv[1], 1);

    thread_data data;
    data.queue = &q;
    data.substring = argv[2];
    pthread_mutex_init(&data.output_mutex, NULL);

    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, worker, &data);
    }

    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    queue_destroy(&q);
    pthread_mutex_destroy(&data.output_mutex);
    return 0;
}