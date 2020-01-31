#include <stdio.h>
#include <pthread.h>
#include "ralloc.h"
#include <stdlib.h>



int deadlock_mode;  // deadlock mode
int process_count;
int resource_count;
int *available_resources;   //exits resources
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //mutex
pthread_cond_t cv;  // condition variable
int **max_demands;  //max demands for each processes
int **allocated_resources;  //allocated resources for processes
int i;
int j;
// this includes printfs for debugging; you will delete them in final version.

int ralloc_init(int p_count, int r_count, int r_exist[], int d_handling)
{
    pthread_mutex_lock(&mutex);
    //Error conditions
    if(p_count < 0 || r_count < 0 || r_exist == NULL || p_count > MAX_PROCESSES ||
        r_count > MAX_RESOURCE_TYPES || d_handling < 1 || d_handling > 3)
    {
        printf("Error for ralloc_init()\n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    deadlock_mode = d_handling;
    pthread_cond_init(&cv, NULL);
    process_count = p_count;
    resource_count = r_count;
    available_resources = (int *)malloc(resource_count * sizeof(int));
    for(i = 0; i < resource_count; i++)
    {
        available_resources[i] = r_exist[i];
    }

    max_demands = (int **)malloc(process_count * sizeof(int*));
    for(i = 0; i < process_count; i++)
    {
        max_demands[i] = (int*)malloc(resource_count * sizeof(int));
    }

    allocated_resources = (int **)malloc(process_count * sizeof(int*));
    for(i = 0; i < process_count; i++)
    {
        allocated_resources[i] = (int*)malloc(resource_count * sizeof(int));
    }
    //make current allocated resources all 0
    for(i = 0; i < process_count; i++)
    {
        for(j = 0; j < resource_count; j++)
        {
            allocated_resources[i][j] = 0;
        }
    }

    pthread_mutex_unlock(&mutex);
    return (0);
}

int ralloc_maxdemand(int pid, int r_max[]){
    pthread_mutex_lock(&mutex);
    if(pid < 0 || pid >= process_count)
    {
        printf("Error: pid must be 0 <= pid < process_count \n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    for(i = 0; i < resource_count; i++)
    {
        if(r_max[i] < 0)
        {
            printf("Error: Max demand can not be negative for process pid: %d \n", pid);
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        max_demands[pid][i] = r_max[i];
    }
    pthread_mutex_unlock(&mutex);
    return (0);
}

int ralloc_request (int pid, int demand[]) {
    //lock
    pthread_mutex_lock(&mutex);
    if(pid < 0 || pid >= process_count)
    {
        printf("Error: pid must be 0 <= pid < process_count \n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    if(deadlock_mode == 1 || deadlock_mode == 2 || deadlock_mode == 3)
    {
        // check if process is trying to allocate more then its max_demand
        for(i = 0; i < resource_count; i++)
        {
            //Return error since process can not allocated more then its max_demand
            if(max_demands[pid][i] < allocated_resources[pid][i] + demand[i])
            {
                printf("Error: process can not allocate more then it's max demand\n");
                pthread_mutex_unlock(&mutex);
                return -1;
            }
        }
        int temp_count = 0;
        while(temp_count != resource_count)
        {
            for(i = 0; i < resource_count; i++)
            {
                if(demand[i] < 0)
                {
                    printf("Error: demand can not be negative\n");
                    pthread_mutex_unlock(&mutex);
                    return -1;
                }
                /*if not enough resource detected wait with condition variable
                and after signal check all resources again.
                */
                if(available_resources[i] < demand[i])
                {
                    pthread_cond_wait(&cv,&mutex);
                    temp_count = 0;
                    break;
                }
                //if current resource is available
                else
                {
                    temp_count++;
                }
            }
        }
        //if we come here we are sure that there is enough resources to allocate

        //-------------Deadlock Avoidance--------------
        if(deadlock_mode == 3)
        {
            //initilize finished_processes
            int *finished_processes = (int *)malloc(process_count * sizeof(int));
            // temp available resources
            int *temp_resources = (int *)malloc(resource_count * sizeof(int));
            //initialize need_for_max
            int **need_for_max; //basically max_demands - allocated_resources
            need_for_max = (int **)malloc(process_count * sizeof(int*));
            for(i = 0; i < process_count; i++)
            {
                need_for_max[i] = (int*)malloc(resource_count * sizeof(int));
            }
            int flag = 0;
            int temp_executing = 1;
            while(flag == 0)
            {
                //-------------setup for banker's algo----------------------------
                //need_for_max values are given
                for(i = 0; i < process_count; i++)
                {
                    //make all finished_processes to 0 which means not finished
                    finished_processes[i] = 0;
                    for(j = 0; j < resource_count; j++)
                    {
                        need_for_max[i][j] = max_demands[i][j] - allocated_resources[i][j];
                    }
                }
                //Allocate desired resources for temporary
                for(i = 0; i < resource_count; i++)
                {
                    temp_resources[i] = available_resources[i] - demand[i];
                    need_for_max[pid][i] = need_for_max[pid][i] - demand[i];
                }
                //---------------End of setup for banker's algo------------------
                temp_count = 0;
                temp_executing = 1;
                while(temp_count != process_count && temp_executing != -1)
                {
                    temp_executing = -1;
                    for(i = 0; i < process_count; i++)
                    {
                        if(finished_processes[i] == 0)
                        {
                            temp_executing = i;
                            for(j = 0; j < resource_count; j++)
                            {
                                if(temp_resources[j] < need_for_max[temp_executing][j])
                                {
                                    temp_executing = -1;
                                    break;
                                }
                            }
                        }
                        if(temp_executing != -1)
                        {
                            temp_count++;
                            finished_processes[temp_executing] = 1;
                            for(j = 0; j < resource_count; j++)
                            {
                                temp_resources[j] += (max_demands[temp_executing][j] - need_for_max[temp_executing][j]);
                                need_for_max[temp_executing][j] = 0;
                            }
                        }
                    }
                }
                //check for safety after banker's algorightm
                flag = 1;
                for(i = 0; i < process_count; i++)
                {
                    // It is not safe to allocate, wait and restart the algorithm after signal
                    if(finished_processes[i] == 0)
                    {
                        flag = 0;
                        printf("Not Safe for allocation of requested resources wait until a release call.\n");
                        pthread_cond_wait(&cv, &mutex);
                        break;
                    }
                }
            }
            //free for memory leak after we are done with algorithm
            free(finished_processes);
            free(temp_resources);
            for(i = 0; i < process_count; i++)
            {
                free(need_for_max[i]);
            }
            free(need_for_max);

        }
        /* If we come to this point it is safe to allocate desired resources
        or we are already not using DEADLOCK_AVOIDANCE algorithm
        */
        for(i = 0; i < resource_count; i++)
        {
            available_resources[i] = available_resources[i] - demand[i];
            allocated_resources[pid][i] = allocated_resources[pid][i] + demand[i];
        }
        printf("Requested resources allocated\n");
        //unlock after allocation
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    //-------------Error------------
    else
    {
        printf("Wrong deadlock_mode initialization\n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }
}
int ralloc_release (int pid, int demand[]) {
    pthread_mutex_lock(&mutex);
    if(pid < 0 || pid >= process_count)
    {
        printf("Error: pid must be 0 <= pid < process_count \n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    for(i = 0; i < resource_count; i++)
    {
        if(demand[i] < 0)
        {
            printf("Error: release demand can not be negative\n");
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        if(allocated_resources[pid][i] < demand[i])
        {
            printf("Error: release demand can not be bigger than allocated resources\n");
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }
    for(i = 0; i < resource_count; i++)
    {
        available_resources[i] = available_resources[i] + demand[i];
        allocated_resources[pid][i] = allocated_resources[pid][i] - demand[i];
    }
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mutex);
    return (0);
}

int ralloc_detection(int procarray[]) {
    //lock
    pthread_mutex_lock(&mutex);
    if(deadlock_mode == 2)
    {
        //initilize finished_processes
        int *finished_processes = (int *)malloc(process_count * sizeof(int));
        // temp available resources
        int *temp_resources = (int *)malloc(resource_count * sizeof(int));
        //initialize need_for_max
        int **need_for_max; //basically max_demands - allocated_resources
        need_for_max = (int **)malloc(process_count * sizeof(int*));
        for(i = 0; i < process_count; i++)
        {
            need_for_max[i] = (int*)malloc(resource_count * sizeof(int));
        }
        int temp_executing = 1;
        //-------------setup for banker's algo----------------------------
        //need_for_max values are given
        for(i = 0; i < process_count; i++)
        {
            //make all finished_processes to 0 which means not finished
            finished_processes[i] = 0;
            for(j = 0; j < resource_count; j++)
            {
                need_for_max[i][j] = max_demands[i][j] - allocated_resources[i][j];
            }
        }
        //Allocate desired resources for temporary
        for(i = 0; i < resource_count; i++)
        {
            temp_resources[i] = available_resources[i];
        }
        //---------------End of setup for banker's algo------------------
        int temp_count = 0;
        temp_executing = 1;
        while(temp_count != process_count && temp_executing != -1)
        {
            temp_executing = -1;
            for(i = 0; i < process_count; i++)
            {
                if(finished_processes[i] == 0)
                {
                    temp_executing = i;
                    for(j = 0; j < resource_count; j++)
                    {
                        if(temp_resources[j] < need_for_max[i][j])
                        {
                            temp_executing = -1;
                            break;
                        }
                    }
                }
                if(temp_executing != -1)
                {
                    temp_count++;
                    finished_processes[temp_executing] = 1;
                    for(j = 0; j < resource_count; j++)
                    {
                        temp_resources[j] += (max_demands[temp_executing][j] - need_for_max[temp_executing][j]);
                        need_for_max[temp_executing][j] = 0;
                    }
                }
            }
        }
        //Check for finished_processes for deadlocks after algorithm
        int deadlock_count = 0;
        for(i = 0; i < process_count; i++)
        {
            // Detect
            if(finished_processes[i] == 0)
            {
                procarray[i] = 1;
                deadlock_count++;
            }
        }

        //free for memory leak after we are done with algorithm
        free(finished_processes);
        free(temp_resources);
        for(i = 0; i < process_count; i++)
        {
            free(need_for_max[i]);
        }
        free(need_for_max);

        pthread_mutex_unlock(&mutex);
        return deadlock_count;
    }
    else
    {
        printf("Error: Can not call ralloc_detection when DEADLOCK_DETECTION is not used.\n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }

}

int ralloc_end() {
    pthread_mutex_lock(&mutex);
    free(available_resources);
    for(i = 0; i < process_count; i++)
    {
        free(max_demands[i]);
        free(allocated_resources[i]);
    }
    free(max_demands);
    free(allocated_resources);
    pthread_mutex_unlock(&mutex);
    return (0);
}
