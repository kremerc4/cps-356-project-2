/* Problem 1: Chef-Waiter system. */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHEFS 5
#define MAX_WAITERS 5
#define MAX_CAPACITY 20

typedef struct {
    int meals[MAX_CAPACITY];
    int capacity, head, tail, count;
    int meals_per_chef, active_chefs;
    int prepared, served, max_usage;
    int aborted;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Restaurant;

typedef struct {
    Restaurant *restaurant;
    int id;
} Worker;

static void *chef(void *argument)
{
    Worker *worker = argument;
    Restaurant *r = worker->restaurant;

    for (int i = 0; i < r->meals_per_chef; ++i) {
        pthread_mutex_lock(&r->mutex);
        /* Recheck space after each wakeup. */
        while (r->count == r->capacity && !r->aborted)
            pthread_cond_wait(&r->not_full, &r->mutex);
        if (r->aborted) {
            pthread_mutex_unlock(&r->mutex);
            return NULL;
        }

        int meal = ++r->prepared;
        r->meals[r->tail] = meal;
        r->tail = (r->tail + 1) % r->capacity;
        ++r->count;
        if (r->count > r->max_usage)
            r->max_usage = r->count;
        printf("Chef %d prepared meal %d (buffer size = %d)\n",
               worker->id, meal, r->count);
        pthread_cond_signal(&r->not_empty);
        pthread_mutex_unlock(&r->mutex);
    }

    pthread_mutex_lock(&r->mutex);
    if (--r->active_chefs == 0)
        /* Wake all waiters when the last chef finishes. */
        pthread_cond_broadcast(&r->not_empty);
    pthread_mutex_unlock(&r->mutex);
    return NULL;
}

static void *waiter(void *argument)
{
    Worker *worker = argument;
    Restaurant *r = worker->restaurant;

    for (;;) {
        pthread_mutex_lock(&r->mutex);
        while (r->count == 0 && r->active_chefs > 0 && !r->aborted)
            pthread_cond_wait(&r->not_empty, &r->mutex);
        if (r->aborted || (r->count == 0 && r->active_chefs == 0)) {
            pthread_mutex_unlock(&r->mutex);
            return NULL;
        }

        int meal = r->meals[r->head];
        r->head = (r->head + 1) % r->capacity;
        --r->count;
        ++r->served;
        printf("Waiter %d served meal %d (buffer size = %d)\n",
               worker->id, meal, r->count);
        pthread_cond_signal(&r->not_full);
        pthread_mutex_unlock(&r->mutex);
    }
}

static int read_input(int values[4])
{
    const long minimum[4] = {1, 1, 5, 1};
    const long maximum[4] = {5, 5, 20, 20};
    char token[64];

    for (int i = 0; i < 4; ++i) {
        if (scanf("%63s", token) != 1)
            return 0;
        char *end;
        errno = 0;
        long value = strtol(token, &end, 10);
        if (errno == ERANGE || end == token || *end != '\0' ||
            value < minimum[i] || value > maximum[i])
            return 0;
        values[i] = (int)value;
    }
    return 1;
}

int main(void)
{
    int input[4];
    if (!read_input(input)) {
        fprintf(stderr, "Expected C W B M: C,W in 1..5, B in 5..20, M in 1..20.\n");
        return EXIT_FAILURE;
    }

    Restaurant r = {
        .capacity = input[2], .meals_per_chef = input[3],
        .active_chefs = input[0],
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .not_empty = PTHREAD_COND_INITIALIZER,
        .not_full = PTHREAD_COND_INITIALIZER
    };
    pthread_t chefs[MAX_CHEFS], waiters[MAX_WAITERS];
    Worker chef_args[MAX_CHEFS], waiter_args[MAX_WAITERS];
    int chefs_started = 0, waiters_started = 0, error = 0;

    for (int i = 0; i < input[1]; ++i) {
        waiter_args[i] = (Worker){&r, i + 1};
        error = pthread_create(&waiters[i], NULL, waiter, &waiter_args[i]);
        if (error != 0)
            goto creation_failed;
        ++waiters_started;
    }
    for (int i = 0; i < input[0]; ++i) {
        chef_args[i] = (Worker){&r, i + 1};
        error = pthread_create(&chefs[i], NULL, chef, &chef_args[i]);
        if (error != 0)
            goto creation_failed;
        ++chefs_started;
    }
    goto join_workers;

creation_failed:
    fprintf(stderr, "Cannot create worker thread: %s\n", strerror(error));
    pthread_mutex_lock(&r.mutex);
    r.aborted = 1;
    pthread_cond_broadcast(&r.not_empty);
    pthread_cond_broadcast(&r.not_full);
    pthread_mutex_unlock(&r.mutex);

join_workers:
    for (int i = 0; i < chefs_started; ++i)
        pthread_join(chefs[i], NULL);
    for (int i = 0; i < waiters_started; ++i)
        pthread_join(waiters[i], NULL);

    if (error == 0) {
        printf("\nExecution Summary\n");
        printf("Total meals prepared: %d\n", r.prepared);
        printf("Total meals served: %d\n", r.served);
        printf("Maximum buffer usage: %d\n", r.max_usage);
    }
    pthread_cond_destroy(&r.not_full);
    pthread_cond_destroy(&r.not_empty);
    pthread_mutex_destroy(&r.mutex);
    return error == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
