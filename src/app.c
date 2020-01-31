#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pthread.h"
#include "ralloc.h"

int handling_method;          // deadlock handling method
int process_counter;

#define M 3                   // number of resource types
int exist[3] =  {12000, 8000, 10000};  // resources existing in the system

#define N 5                   // number of processes - threads
pthread_t tids[N];            // ids of created threads

void *aprocess (void *p)
{
    int req[3];
    int k;
    int pid;

    pid =  *((int *)p);

    printf ("this is thread %d\n", pid);
    fflush (stdout);

    req[0] = 5000;
    req[1] = 5000;
    req[2] = 5000;
    ralloc_maxdemand(pid, req);


    req[0] = 2;
    req[1] = 2;
    req[2] = 2;
    for(k = 0; k < 1000; k++)
        ralloc_request (pid, req);
    // do something with the resources
    //ralloc_release (pid, req);

    // call request and release as many times as you wish with
    // different parameters
    //sleep(10);

    req[0] = 2;
    req[1] = 2;
    req[2] = 2;
    for(k = 0; k < 1000; k++)
    {
        ralloc_release (pid, req);
        printf("Release\n");
    }

    // call request and release as many times as you wish with
    // different parameters
    process_counter += 1;
    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    int dn; // number of deadlocked processes
    int deadlocked[N]; // array indicating deadlocked processes
    int k;
    int i;
    int pids[N];
    process_counter = 0;

    for (k = 0; k < N; ++k)
        deadlocked[k] = -1; // initialize

    handling_method = DEADLOCK_AVOIDANCE;   //change here for different handlings
    ralloc_init (N, M, exist, handling_method);

    printf ("library initialized\n");
    fflush(stdout);

    for (i = 0; i < N; ++i) {
        pids[i] = i;
        pthread_create (&(tids[i]), NULL, (void *) &aprocess,
                        (void *)&(pids[i]));
    }

    printf ("threads created = %d\n", N);
    fflush (stdout);

    while (1) {
        //sleep (5); // detection period
        if (handling_method == DEADLOCK_DETECTION) {
            dn = ralloc_detection(deadlocked);
            if (dn > 0) {
                printf ("there are deadlocked processes\n");
                for(k = 0; k < N; k++)
                {
                    if(deadlocked[k] == 1)
                    {
                        printf("PID: %d is deadlocked.\n", k);
                    }
                }
            }
        }
        // write code for:
        // if all treads terminated, call ralloc_end and exit
        if(process_counter == N)
        {
            printf("All threads terminated. \n");
            ralloc_end();
            return 0;
        }
    }
}
