#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <math.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <time.h>

int mem_id; 
int semaphores;

struct SharedMemory {
    int * number;
    struct timeval * timestamp;
} shared_mem;

int semP(int semid,int index){ //sem down
    struct sembuf sem_b; 
    int op;
    sem_b.sem_num = index;
    sem_b.sem_op = -1;
    sem_b.sem_flg = 0;
  op = semop(semid,&sem_b,1);
  return op;
}

int semV(int semid,int index){ //sem up
  int op;
    struct sembuf sem_b;  
    sem_b.sem_num = index;
    sem_b.sem_op = 1;
    sem_b.sem_flg = 0;
    op = semop(semid,&sem_b,1);
    return op;
}

int createSemaphores(int num){
    if ((semaphores = semget(IPC_PRIVATE, num, IPC_CREAT | 0600)) < 0) {
        perror("creating semaphore");
        exit(EXIT_FAILURE);
    }
  return semaphores;
}

void initializeSemaphores(int semid,int index,int val){
  int init = semctl(semid, index, SETVAL,val);
  if (init < 0){
    perror("Fail to initialize semaphores");
    exit(1);
  }
}

void removeSemaphores(int sem_id){
  int rem = semctl(sem_id,0,IPC_RMID);
  if (rem < 0)
   {  perror ("error in removing semaphore from the system");
      exit (1);
   }
}

int main(int argc, char** argv) {
    int i,j;
    int n = atoi(argv[1]);
    int M = atoi(argv[2]);
    int* array = malloc(sizeof(int)*M);
    //create shared memory
    if ((mem_id = shmget(IPC_PRIVATE, sizeof(struct SharedMemory), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) == -1) {
        perror("shmget");
        exit(1);
    }
    //attach
    shared_mem.number = (int*) shmat(mem_id,NULL, 0);
    //offset for pointers
    shared_mem.number[0] = 0;
    shared_mem.timestamp = (struct timeval *) (((char*) shared_mem.number) + sizeof (int));

    semaphores = createSemaphores(n + 1);
    initializeSemaphores(semaphores,0,n);
    int l;
    for(l =1; l<=n; l++)
        initializeSemaphores(semaphores,l,0);
    pid_t * pids = malloc(sizeof(pid_t)*n);
    for (i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i]<0){
            perror("fork failed");
        }
        if (pids[i] == 0) { //readers
            int * array= malloc(M * sizeof (int));
            for (j = 0; j < M; j++)
                array[j] = 0;
            double time_spend = 0;
            char filename[3000];
            sprintf(filename,"myfile%d.txt",i);
            for (j = 0; j < M; j++) { // for each number
                struct timeval tv;
                double time_block;
                int index = i+1;
                semP(semaphores, index);// the children are blocked until the father writes  an int
                //when are unblocked, they read 
                array[j] =shared_mem.number[0];
                printf("child %d read %d\n",getpid(),shared_mem.number[0]);
                gettimeofday(&tv, NULL);
                time_block = (double) (tv.tv_usec - shared_mem.timestamp->tv_usec) / 10000000 + (double) (tv.tv_sec - shared_mem.timestamp->tv_sec);
                time_spend = time_spend + time_block;
                semV(semaphores, 0); //last child unblock father
            }
            time_spend = time_spend / n; //average time
            if(i == n - 1 ){ //last process makes the file
                FILE * fp = fopen(filename, "w");
                for (j = 0; j < M; j++)
                    fprintf(fp, "array[%d] = %d \n", j, array[j]);
                fprintf(fp, "pid  = %d \n", getpid());
                fprintf(fp, "average time =  %lf \n", time_spend);
                printf("AVERAGE TIME = %f\n",time_spend);
                fclose(fp);
            }
            exit(0);
        }
    }
    srand(time(NULL));
    for(i=0; i< M; i++)
        array[i] = rand();
    for (int i = 0; i < M; ++i)
        printf("%d\n", array[i]);
    for (i = 0; i < M; i++){
        int k;
        for(k = 1; k<=n; k++) //n times (to unblock father)
            semP(semaphores,0); //
        gettimeofday(shared_mem.timestamp, NULL); 
        printf("feeder writes %d\n",array[i] );
        shared_mem.number[0] =  array[i] ;
        for (j = 0; j < n; j++) { //father unblock each child
            int index = j+1;
            semV(semaphores, index); 
        }
    }
    int status;
    while (wait(&status) > 0);
    wait(&status); 

    shmdt(&shared_mem);
    removeSemaphores(semaphores);
    shmctl(mem_id, IPC_RMID, 0);
    free(pids);
    free(array);
    return 0;
}


