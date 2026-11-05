/*
 * Dining Philosophers Simulation
 *
 * Build: gcc -o diningPhilosophers main.c -Wall -pthread
 * Run:   ./diningPhilosophers
 *
 * No deadlock: even philosophers pick up their right fork first and odd
 * philosophers pick up their left fork first, which breaks circular wait
 *
 * No starvation: each fork works like a ticket lock, so philosophers get
 * the fork in the order they asked for it
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// set  number of philosophers, for project example valid numbers of N are: 5, 6, 7
#define N 5
// longest possible time (in microseconds) that a philosopher can spend in any state
#define MAX_DELAY 800000
// create artificial delay between states for better readability
#define FRAME_DELAY 500000

// define different philosopher states
#define THINKING 0
#define HUNGRY 1
#define EATING 2
#define DONE 3

// use to keep track of free forks
#define NO_ONE -1

// struct for fork state
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int nextTicket;
    int nowServing;
} Fork;

// initialize fork state array and pthreads
Fork forks[N];
pthread_mutex_t displayLock = PTHREAD_MUTEX_INITIALIZER;

// keep track of key philosopher states
int state[N];
int meals[N];
int forkOwner[N];

// number of times each philosopher eats
int cycles;

// keeps track of how many state updates have occured
int step = 0;

// for formatted printing
const char *stateNames[] = {"Thinking", "Hungry", "Eating", "Done"};

// get left fork (from slides)
int left(int p) {
    return p;
}

// get right fork (from slides)
int right(int p) {
    return (p + 1) % N;
}

// logic for getting a fork when available
// utilizes ticket style logic to avoid starvation
void getFork(int f) {
    pthread_mutex_lock(&forks[f].lock);
    int myTicket = forks[f].nextTicket++;
    while (forks[f].nowServing != myTicket) {
        pthread_cond_wait(&forks[f].cond, &forks[f].lock);
    }
    pthread_mutex_unlock(&forks[f].lock);
}

// logic for releasing a fork and increasing ticket so next person can get fork
void putFork(int f) {
    pthread_mutex_lock(&forks[f].lock);
    forks[f].nowServing++;
    pthread_cond_broadcast(&forks[f].cond);
    pthread_mutex_unlock(&forks[f].lock);
}

// draw current state in formatted ascii display
// call only while holding displayLock to ensure sequential printing
void draw(const char *event, int p) {
    // store each philosopher state for output
    char label[N][40];

    // format each philosopher state
    for (int i = 0; i < N; i++) {
        sprintf(label[i], "Philosopher %d (%s) %d/%d",
                i + 1, stateNames[state[i]], meals[i], cycles);
    }

    step++;

    if (N == 5) {
        // print formatted ascii update to philosophers' states
        printf("\n--- Step %d: Philosopher %d %s ---\n\n", step, p + 1, event);
        printf("%22s%s\n", "", label[0]);
        printf("%14s[F1]%28s[F2]\n\n", "", "");
        printf("%-38s%s\n\n", label[4], label[1]);
        printf("%14s[F5]%28s[F3]\n", "", "");
        printf("%-30s  [F4]  %s\n\n", label[3], label[2]);
    } else if (N == 6) {
        // print formatted ascii update to philosophers' states
        printf("\n--- Step %d: Philosopher %d %s ---\n\n", step, p + 1, event);
        printf("%-30s  [F2]  %s\n\n", label[0], label[1]);
        printf("%14s[F1]%28s[F3]\n\n", "", "");
        printf("%-38s%s\n\n", label[5], label[2]);
        printf("%14s[F6]%28s[F4]\n", "", "");
        printf("%-30s  [F5]  %s\n\n", label[4], label[3]);
    } else if (N == 7) {
        // print formatted ascii update to philosophers' states
        printf("\n--- Step %d: Philosopher %d %s ---\n\n", step, p + 1, event);
        printf("%22s%s\n", "", label[0]);
        printf("%14s[F1]%28s[F2]\n\n", "", "");
        printf("%-38s%s\n\n", label[6], label[1]);
        printf("%14s[F7]%28s[F3]\n", "", "");
        printf("%-38s%s\n\n", label[5], label[2]);
        printf("%14s[F6]%28s[F4]\n", "", "");
        printf("%-30s  [F5]  %s\n\n", label[4], label[3]);
    } else {
        printf("INVALID NUMBER OF PHILOSOPHERS");
    }

    // print fork statuses
    for (int f = 0; f < N; f++) {
        if (forkOwner[f] == NO_ONE) {
            printf("F%d: Available\n", f + 1);
        } else {
            printf("F%d: In use by Philosopher %d\n", f + 1, forkOwner[f] + 1);
        }
    }

    // ensure clean output
    fflush(stdout);

    // delay after print (avoid clutter)
    usleep(FRAME_DELAY);
}


void pickUp(int p, int f) {
    // store fork pick up state for output
    char event[32];

    // get fork when available
    getFork(f);

    // lock and print for consistent output
    pthread_mutex_lock(&displayLock);
    forkOwner[f] = p;
    sprintf(event, "picked up F%d", f + 1);
    draw(event, p);
    pthread_mutex_unlock(&displayLock);
}

// logic for deciding which forks any philosopher should get
// uses ordering based on even/odd philosopher to avoid deadlocks
void getForks(int p) {
    if (p % 2 == 0) { // if even philosopher pick up right fork then left
        pickUp(p, right(p));
        pickUp(p, left(p));
    } else { // if odd philosopher pick up left fork then right
        pickUp(p, left(p));
        pickUp(p, right(p));
    }
}

// logic for putting forks back after finished eating
void putForks(int p) {
    pthread_mutex_lock(&displayLock);
    
    // reset owners
    forkOwner[left(p)] = NO_ONE;
    forkOwner[right(p)] = NO_ONE;

    // unlock forks so a fork shown as available is really free
    putFork(left(p));
    putFork(right(p));

    // update philosopher's state
    if (meals[p] == cycles) { // finished eating if has ate for number of cycles
        state[p] = DONE;
        draw("put down both forks and is done", p);
    } else { // return to thinking if not ate for number of cycles
        state[p] = THINKING;
        draw("put down both forks", p);
    }
    pthread_mutex_unlock(&displayLock);
}

// simulate each philosopher's state
void *philosopher(void *arg) {
    // parse philosopher number from arg
    int p = *(int *) arg;

    // get seed for rand (use different seed for each philosopher)
    unsigned int seed = p + 1;

    // main state simulation
    for (int i = 0; i < cycles; i++) {
        // think for random amount of time (limited by max)
        usleep(rand_r(&seed) % MAX_DELAY);

        // when finished thinking, update state to hungry
        pthread_mutex_lock(&displayLock);
        state[p] = HUNGRY;
        draw("is hungry", p);
        pthread_mutex_unlock(&displayLock);

        // get forks to eat
        getForks(p);

        // when has both forks, start eating
        pthread_mutex_lock(&displayLock);
        state[p] = EATING;
        meals[p]++;
        draw("is eating", p);
        pthread_mutex_unlock(&displayLock);

        // eat for random amount of time (limited by max)
        usleep(rand_r(&seed) % MAX_DELAY);

        // put back forks
        putForks(p);
    }

    // return null to indicate finished
    return NULL;
}

int main(void) {
    // initialize threads and ids
    pthread_t threads[N];
    int ids[N];

    // scan in eat cycles (must be single integer and positive)
    if (scanf("%d", &cycles) != 1 || cycles <= 0) {
        printf("T must be a positive integer.\n");
        return 1;
    }

    // initialize all states
    for (int i = 0; i < N; i++) {
        pthread_mutex_init(&forks[i].lock, NULL);
        pthread_cond_init(&forks[i].cond, NULL);
        forks[i].nextTicket = 0;
        forks[i].nowServing = 0;
        state[i] = THINKING;
        meals[i] = 0;
        forkOwner[i] = NO_ONE;
    }

    // start all philosopher cycles
    for (int i = 0; i < N; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }

    // join as each philosopher finishes each eatting cycle (philosopher function returns null)
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    // print that finished once all threads joined
    printf("\nAll philosophers are done.\n");
    return 0;
}