/*
 * Dining Philosophers Simulation
 *
 * Build: gcc -o diningPhilosophers diningPhilosophers.c -Wall -pthread
 * Run:   ./diningPhilosophers
 *
 * No deadlock: even philosophers pick up their right fork first and odd
 * philosophers pick up their left fork first, which breaks circular wait.
 *
 * No starvation: each fork works like a ticket lock, so philosophers get
 * the fork in the order they asked for it.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N 5
#define MAX_DELAY 800000     /* longest think or eat time, in microseconds */
#define FRAME_DELAY 400000   /* pause after each table */

#define THINKING 0
#define HUNGRY 1
#define EATING 2
#define DONE 3

#define NO_ONE -1

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int nextTicket;
    int nowServing;
} Fork;

Fork forks[N];
pthread_mutex_t displayLock = PTHREAD_MUTEX_INITIALIZER;

int state[N];
int meals[N];
int forkOwner[N];
int cycles;
int step = 0;

const char *stateNames[] = {"Thinking", "Hungry", "Eating", "Done"};

int left(int p) {
    return p;
}

int right(int p) {
    return (p + 1) % N;
}

void getFork(int f) {
    pthread_mutex_lock(&forks[f].lock);
    int myTicket = forks[f].nextTicket++;
    while (forks[f].nowServing != myTicket) {
        pthread_cond_wait(&forks[f].cond, &forks[f].lock);
    }
    pthread_mutex_unlock(&forks[f].lock);
}

void putFork(int f) {
    pthread_mutex_lock(&forks[f].lock);
    forks[f].nowServing++;
    pthread_cond_broadcast(&forks[f].cond);
    pthread_mutex_unlock(&forks[f].lock);
}

/* Call this only while holding displayLock. */
void draw(const char *event, int p) {
    char label[N][40];

    for (int i = 0; i < N; i++) {
        sprintf(label[i], "Philosopher %d (%s) %d/%d",
                i + 1, stateNames[state[i]], meals[i], cycles);
    }

    step++;
    printf("\n--- Step %d: Philosopher %d %s ---\n\n", step, p + 1, event);
    printf("%22s%s\n", "", label[0]);
    printf("%14s[F1]%28s[F2]\n\n", "", "");
    printf("%-38s%s\n\n", label[4], label[1]);
    printf("%14s[F5]%28s[F3]\n", "", "");
    printf("%-30s  [F4]  %s\n\n", label[3], label[2]);

    for (int f = 0; f < N; f++) {
        if (forkOwner[f] == NO_ONE) {
            printf("F%d: Available\n", f + 1);
        } else {
            printf("F%d: In use by Philosopher %d\n", f + 1, forkOwner[f] + 1);
        }
    }
    fflush(stdout);
    usleep(FRAME_DELAY);
}

void pickUp(int p, int f) {
    char event[32];

    getFork(f);

    pthread_mutex_lock(&displayLock);
    forkOwner[f] = p;
    sprintf(event, "picked up F%d", f + 1);
    draw(event, p);
    pthread_mutex_unlock(&displayLock);
}

void getForks(int p) {
    if (p % 2 == 0) {
        pickUp(p, right(p));
        pickUp(p, left(p));
    } else {
        pickUp(p, left(p));
        pickUp(p, right(p));
    }
}

void putForks(int p) {
    pthread_mutex_lock(&displayLock);
    forkOwner[left(p)] = NO_ONE;
    forkOwner[right(p)] = NO_ONE;

    /* Unlock the forks first so a fork shown as Available is really free. */
    putFork(left(p));
    putFork(right(p));

    if (meals[p] == cycles) {
        state[p] = DONE;
        draw("put down both forks and is done", p);
    } else {
        state[p] = THINKING;
        draw("put down both forks", p);
    }
    pthread_mutex_unlock(&displayLock);
}

void *philosopher(void *arg) {
    int p = *(int *) arg;
    unsigned int seed = p + 1;

    for (int i = 0; i < cycles; i++) {
        usleep(rand_r(&seed) % MAX_DELAY);    /* think */

        pthread_mutex_lock(&displayLock);
        state[p] = HUNGRY;
        draw("is hungry", p);
        pthread_mutex_unlock(&displayLock);

        getForks(p);

        pthread_mutex_lock(&displayLock);
        state[p] = EATING;
        meals[p]++;
        draw("is eating", p);
        pthread_mutex_unlock(&displayLock);

        usleep(rand_r(&seed) % MAX_DELAY);    /* eat */

        putForks(p);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[N];
    int ids[N];

    if (scanf("%d", &cycles) != 1 || cycles <= 0) {
        printf("T must be a positive integer.\n");
        return 1;
    }

    for (int i = 0; i < N; i++) {
        pthread_mutex_init(&forks[i].lock, NULL);
        pthread_cond_init(&forks[i].cond, NULL);
        forks[i].nextTicket = 0;
        forks[i].nowServing = 0;
        state[i] = THINKING;
        meals[i] = 0;
        forkOwner[i] = NO_ONE;
    }

    for (int i = 0; i < N; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nAll philosophers are done.\n");
    return 0;
}