#include <sys/ipc.h> 
#include <sys/types.h> 
#include <sys/msg.h> 
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <semaphore.h>

#define NUM_PROCESSES 4

struct mInfo{ 
    long priority; //message priority 
    int temp; //temperature 
    int pid; //process id 
    int stable; //boolean for temperature stability 
};

struct mInfo msgp[2]; 
struct mInfo cmbox[2];

struct pInfo{ 
    int mailbox; 
    int initTemp; 
    int p_Num; 
    int p_Range; 
    int counter; 
    int g_Num; 
};

int j = 0;

void *calc_temp(void * arg) { 
    struct pInfo *p = (struct pInfo*)arg;
    //Set up local variables 
    //counter for loops
    int i, result, length, status, temperature;
    
    //central process ID
    int uid = 0;
    
    //mailbox IDs for all processes 
    int msqid[NUM_PROCESSES];
    
    //boolean to denote temp stability 
    int unstable = 1;
    
    //array of process temperatures 
    int tempAry[NUM_PROCESSES];
    
    //Create the Central Servers Mailbox 
    int msqidC = msgget(p->mailbox, 0600 | IPC_CREAT);
    
    for(i = p->p_Num; i <= p->p_Range; i++){ 
        msqid[(p->counter-1)] = msgget((p->mailbox + i), 0600 | IPC_CREAT); 
        p->counter++; 
    }
    
    //Initialize the message to be sent 
    msgp[p->g_Num].priority = 1; 
    msgp[p->g_Num].pid = uid;
    
    msgp[p->g_Num].temp = p->initTemp;
    msgp[p->g_Num].stable = 1;
    
    /* The length is essentially the size of the structure minus sizeof(mtype) */ 
    length = sizeof(struct mInfo) - sizeof(long);
    
    //While the processes have different temperatures 
    while(unstable == 1){
        int sumTemp = 0; //sum up the temps as we loop 
        int stable = 1; //stability trap
        
        // Get new messages from the processes 
        for(i = 0; i < NUM_PROCESSES; i++){ 
            result = msgrcv( msqidC, &cmbox[p->g_Num], 
            length, 1, 0);
            
            /* If any of the new temps are different from the old temps then we are still unstable. Set the new temp to the corresponding process ID in the array */
            if(tempAry[(cmbox[p->g_Num].pid - 1)] != cmbox[p->g_Num].temp) { 
                stable = 0; 
                tempAry[(cmbox[p->g_Num].pid - 1)] = cmbox[p->g_Num].temp; 
            }
            
            //Add up all the temps as we go for the temperature algorithm 
            sumTemp += cmbox[p->g_Num].temp;
        }
        
        /*When all the processes have the same temp twice: 1) Break the loop 2) Set the messages stable field to stable*/
        if(stable){ 
            unstable = 0; 
            msgp[p->g_Num].stable = 0; 
        }
        else { //Calculate a new temp and set the temp field in the message 
            int newTemp = (msgp[p->g_Num].temp + 1000*sumTemp) / (1000*NUM_PROCESSES + 1);
            usleep(100000); 
            msgp[p->g_Num].temp = newTemp; 
            printf("The new temp in GROUP %d is: %d\n",p->g_Num,newTemp); 
            temperature = newTemp; 
        }
        
        /* Send a new message to all processes to inform of new temp or stability */ 
        for(i = 0; i < NUM_PROCESSES; i++){ 
            result = msgsnd( msqid[i], &msgp[p->g_Num], length, 0); 
        }
    }
    
    //Remove the mailbox 
    status = msgctl(msqidC, IPC_RMID, 0);
    
    //Validate nothing when wrong when trying to remove mailbox 
    if(status != 0){ 
        printf("\nERROR closing mailbox\n"); 
    } 
    
    pthread_exit(temperature);
}

int main(int argc, char *argv[]) { 
    struct timeval t1, t2; 
    double elapsedTime;
    
    // start timer 
    gettimeofday(&t1, NULL);
    
    int i = 0; 
    int tempg1, tempg2;
    
    printf("\nStarting Server...\n");
    
    //Validate that a temperature was given via the command line 
    if(argc != 5) { 
        printf("USAGE: Too few arguments --./central.out Temp"); 
        exit(0); 
    }
    
    struct pInfo process[2];
    
    /* First Group */ 
    process[0].mailbox = atoi(argv[3]); 
    process[0].initTemp = atoi(argv[1]); 
    process[0].p_Num = 1; 
    process[0].p_Range = 4; 
    process[0].counter = 1; 
    process[0].g_Num = 0;
    
    /* Second Group */ 
    process[1].mailbox = atoi(argv[4]); 
    process[1].initTemp = atoi(argv[2]); 
    process[1].p_Num = 5; 
    process[1].p_Range = 8; 
    process[1].counter = 1; 
    process[1].g_Num = 1;
    
    pthread_t thread[2];
    
    /* Thread Creation */ 
    pthread_create(&thread[0], NULL, &process, &process[0]); 
    pthread_create(&thread[1], NULL, &process, &process[1]);
    
    /* Wait for all threads to finish before continuing */
    pthread_join(thread[0], (void **)&tempg1); 
    pthread_join(thread[1], (void **)&tempg2);
    
    printf("Temperature Stabilized in Group1: %d \n", tempg1); 
    printf("Temperature Stabilized in Group2: %d \n", tempg2);
    
    // stop timer 
    gettimeofday(&t2, NULL);
    
    // compute and print the elapsed time in millisec 
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms 
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
    
    printf("The elapsed time is %fms\n", elapsedTime);
    
    return EXIT_SUCCESS;
}