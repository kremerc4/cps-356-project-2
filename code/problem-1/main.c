/*
 * Chef-Waiter Restaurant System
 *
 * Build: gcc -o restaurant main.c -Wall -pthread
 * Run:   ./restaurant
 * Input: C W B M (chefs, waiters, buffer capacity, meals per chef)
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHEFS 5
#define MAX_WAITERS 5
#define MAX_CAPACITY 20

// shared buffer and counters, protected by the mutex
typedef struct {
    int meals[MAX_CAPACITY];
    int capacity, head, tail, count;
    int mealsPerChef, activeChefs;
    int prepared, served, maxUsage;
    int aborted;
    pthread_mutex_t mutex;
    pthread_cond_t notEmpty;
    pthread_cond_t notFull;
} Restaurant;

// data passed to each worker
typedef struct {
    Restaurant *restaurant;
    int id;
} Worker;

// prepare and add meals to the buffer
static void *chef(void *argument) {
    Worker *worker = argument;
    Restaurant *r = worker->restaurant;

    for (int i = 0; i < r->mealsPerChef; ++i) {
        pthread_mutex_lock(&r->mutex);
        // wait until the buffer has space
        while (r->count == r->capacity && !r->aborted)
            pthread_cond_wait(&r->notFull, &r->mutex);
        if (r->aborted) {
            pthread_mutex_unlock(&r->mutex);
            return NULL;
        }

        // give each meal a unique ID and add it at the tail
        int meal = ++r->prepared;
        r->meals[r->tail] = meal;
        r->tail = (r->tail + 1) % r->capacity;
        ++r->count;
        if (r->count > r->maxUsage)
            r->maxUsage = r->count;
        printf("Chef %d prepared meal %d (buffer size = %d)\n",
               worker->id, meal, r->count);
        pthread_cond_signal(&r->notEmpty);
        pthread_mutex_unlock(&r->mutex);
    }

    pthread_mutex_lock(&r->mutex);
    if (--r->activeChefs == 0)
        // wake all waiters when the last chef finishes
        pthread_cond_broadcast(&r->notEmpty);
    pthread_mutex_unlock(&r->mutex);
    return NULL;
}

// take meals from the buffer and serve them
static void *waiter(void *argument) {
    Worker *worker = argument;
    Restaurant *r = worker->restaurant;

    for (;;) {
        pthread_mutex_lock(&r->mutex);
        // wait for a meal while chefs are still working
        while (r->count == 0 && r->activeChefs > 0 && !r->aborted)
            pthread_cond_wait(&r->notEmpty, &r->mutex);
        // stop when all meals are served or an error occurs
        if (r->aborted || (r->count == 0 && r->activeChefs == 0)) {
            pthread_mutex_unlock(&r->mutex);
            return NULL;
        }

        // remove the oldest meal
        int meal = r->meals[r->head];
        r->head = (r->head + 1) % r->capacity;
        --r->count;
        ++r->served;
        printf("Waiter %d served meal %d (buffer size = %d)\n",
               worker->id, meal, r->count);
        pthread_cond_signal(&r->notFull);
        pthread_mutex_unlock(&r->mutex);
    }
}

// read and check the four input values
static int readInput(int values[4]) {
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

int main(void) {
    int input[4];
    if (!readInput(input)) {
        fprintf(stderr, "Expected C W B M: C,W in 1..5, B in 5..20, M in 1..20.\n");
        return EXIT_FAILURE;
    }

    // initialize the buffer and synchronization objects
    Restaurant r = {
        .capacity = input[2], .mealsPerChef = input[3],
        .activeChefs = input[0],
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .notEmpty = PTHREAD_COND_INITIALIZER,
        .notFull = PTHREAD_COND_INITIALIZER
    };
    pthread_t chefs[MAX_CHEFS], waiters[MAX_WAITERS];
    Worker chefArgs[MAX_CHEFS], waiterArgs[MAX_WAITERS];
    int chefsStarted = 0, waitersStarted = 0, error = 0;

    // start the waiters and chefs
    for (int i = 0; i < input[1]; ++i) {
        waiterArgs[i] = (Worker){&r, i + 1};
        error = pthread_create(&waiters[i], NULL, waiter, &waiterArgs[i]);
        if (error != 0)
            goto creationFailed;
        ++waitersStarted;
    }
    for (int i = 0; i < input[0]; ++i) {
        chefArgs[i] = (Worker){&r, i + 1};
        error = pthread_create(&chefs[i], NULL, chef, &chefArgs[i]);
        if (error != 0)
            goto creationFailed;
        ++chefsStarted;
    }
    goto joinWorkers;

creationFailed:
    // stop any workers that already started
    fprintf(stderr, "Cannot create worker thread: %s\n", strerror(error));
    pthread_mutex_lock(&r.mutex);
    r.aborted = 1;
    pthread_cond_broadcast(&r.notEmpty);
    pthread_cond_broadcast(&r.notFull);
    pthread_mutex_unlock(&r.mutex);

joinWorkers:
    // wait for all started workers to finish
    for (int i = 0; i < chefsStarted; ++i)
        pthread_join(chefs[i], NULL);
    for (int i = 0; i < waitersStarted; ++i)
        pthread_join(waiters[i], NULL);

    // print the summary after all workers finish
    if (error == 0) {
        printf("\nExecution Summary\n");
        printf("Total meals prepared: %d\n", r.prepared);
        printf("Total meals served: %d\n", r.served);
        printf("Maximum buffer usage: %d\n", r.maxUsage);
    }
    pthread_cond_destroy(&r.notFull);
    pthread_cond_destroy(&r.notEmpty);
    pthread_mutex_destroy(&r.mutex);
    return error == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
