#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<semaphore.h>
#include<unistd.h>
#include<assert.h>
#include<errno.h>
#include<signal.h>
#include<time.h>

#define SZ(x) (sizeof(x)/(sizeof(x)/sizeof(int)))

typedef struct PCB_st{
    int PR;
    int numCPUB, numIOB;
    int *CPUBurst, *IOBurst;
    int cpuindex, ioindex;
    double readyQ, ioQ;
    clock_t enterQ, enterioQ;
    struct PCB *prev;
    struct PCB *next;
} PCB;

int CPUreg[8] ={0};
struct PCB_st *Head=NULL;
struct PCB_st *Tail=NULL;
struct PCB_st *IOHead=NULL;
struct PCB_st *IOTail=NULL;
int CLOCK=0;
int Total_waiting_time=0;
int Total_turnaround_time=0;
int Total_job=0;
int cpu_util=0;
int cpu_busy=0, io_busy=0;
int read_file_done=0;
double cpurq=0;
sem_t sem_cpu, sem_io;
char *algo, *input, *quantum;
struct timespec sec;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void *FileRead_Thread(void *unused) {
    FILE * F;
    char buffer[64];
    char *token, *cpy;
    int pr, nums, sleep;
    sec.tv_sec =1;
    F = fopen(input, "r");
    if(F == NULL) {
        printf("Failed to open file: %s\n", input);
        exit(1);
    }
    while (fgets(buffer, 64, F)) {
        cpy = buffer;
        token = strtok_r(cpy, " ", &cpy);
        //printf("%s\n",token);
        if (strcmp(token, "proc") == 0) {
            pr = atoi(strtok_r(cpy, " ", &cpy));
            nums = atoi(strtok_r(cpy, " ", &cpy));
            //printf("PR %d, nums %d\n",pr, nums);
            pthread_mutex_lock(&lock);
            makeNode(pr, nums, cpy);
            //printf("Node made\n");
            pthread_mutex_unlock(&lock);
            sem_post(&sem_cpu); 
        }
        else if (strcmp(token, "sleep") == 0) {
            sleep = atoi(strtok_r(cpy, " ", &cpy));
            //printf("Read sleep\n");
            usleep(sleep * 1000);
            //printf("gotosleep:7:\n");

            //sem_post(&testsleep);
            //sleep(7);
            //sem_post(&testsleep);
            //printf("Done waiting 7 secs\n");
        }
        else if (strcmp(token, "stop\n") == 0 || strcmp(token, "stop") == 0) {
            read_file_done = 1;
            //printf("READING DONE NOW TERMINATINGQQQQ!!!!\n");
        }
    }
    //printf("read Ends Thread\n");
}

void *CPU_scheduler_thread(void *args) {
    //sem_wait(&read_to_cpu); //tells cpu to do its thing
    int res;

    //printf("THIS SHOULD EXE RIGHT NOW!\N");
    //sem_post(&cpu_to_io);
    //printf("IO TO CPU WORKS\n");
    //return;
    while(1) {
        if(Head == NULL && IOHead == NULL && io_busy == 0 && read_file_done == 1) {
            //printf("Broke CPU loop\n");
            break;
        }
        res = sem_timedwait(&sem_cpu, &sec);
        if(res==-1 && errno==ETIMEDOUT) continue;
        cpu_busy=1;
        //printf("CPU CALLING sched\n");
        pthread_mutex_lock(&lock);
        Total_job += 1;
        scheduler();
        pthread_mutex_unlock(&lock);
        cpu_busy=0;
        sem_post(&sem_io);
        //printf("CEEEPEEUOOO did some things in sch3d\n");
    }
}

void *IO_system_thread(void *args) { 
    int res;
    while(1) {
        if(Head == NULL && IOHead == NULL && cpu_busy == 0 && read_file_done ==1) {
            //printf("Broke IO loop\n");
            break;
        }
        res = sem_timedwait(&sem_io, &sec);
        if(res==-1 && errno==ETIMEDOUT) continue;
        pthread_mutex_lock(&lock);
        io_busy = 1;
        Total_job +=1;
        ioscheduler();
        io_busy = 0;
        pthread_mutex_unlock(&lock);
        sem_post(&sem_cpu);
        //printf("I/O      This after read thread fell asleep\n");
    }
}
//called by cpu and io, calls scheduling algorithms
void scheduler() {
    if(Head == NULL && Tail == NULL) return;
    if(strcmp(algo, "FIFO") == 0)
        FIFO_Scheduling(); 
    else if (strcmp(algo, "SJF") == 0)
    {}    //SJF_Scheduling();
    else if (strcmp(algo, "PR") == 0)
    {}    //PR_Scheduling();
    else if (strcmp(algo, "RR") == 0)
    {}    //RR_Scheduling();
    else {
        fprintf(stderr, "Failed to provide algorithm: -alg \"algorithm\"\n");
        exit(1);
    }
}

void ioscheduler() {
    if(IOHead == NULL && IOTail == NULL) 
        return;
    //printf("io sched\n");
    struct PCB_st *current = IOHead;
    if(IOHead == IOTail) IOTail = NULL;
    IOHead = IOHead->next;
    current->next = NULL;
    current->prev = NULL;
    int sleep = current->IOBurst[current->ioindex];
    current->ioindex += 1;
    CLOCK += sleep;
    //printf("%d, %d, %d\n",current->PR, sleep, current->ioindex);
    usleep(sleep * 1000);
    //printf("PUSHING TO RQ FROM IO\n");
    //printf("head %d \n", Head->PR);
    //printf("Tail %d\n", Tail->PR);
    //if(Head == NULL) setHead();
    //if(Tail == NULL) setTail();
    //printf("NOW BA%TMAN");
    if(Head == NULL && Tail == NULL) {
        Head = current;
        Tail = current;
    }
    push(current);
    //printf("DONE PUSHING FROM IO TO RQ\n");

}

//first of RQhead is used
void FIFO_Scheduling() {
    struct PCB_st *current = Head;
    //printf("Init Head in FIFO\n");
    //if(Head == NULL && Tail == NULL) return;
    if (Head == Tail) Tail = NULL;
    Head = Head->next;
    current->next = NULL;
    current->prev = NULL;
    int sleep = current->CPUBurst[current->cpuindex];
    current->cpuindex += 1;
    cpu_util += sleep;
    CLOCK += sleep;
    //printf("sleep %d %d\n", sleep, current->PR);
    usleep(sleep * 1000);
    current->readyQ += (double)(clock()) - current->enterQ;
    cpurq += current->readyQ;
    //printf("After sleep\n");

    if(current->cpuindex >= current->numCPUB) {
        //printf("This should NEVER EXECUTE!\n");
        free(current->CPUBurst);
        free(current->IOBurst);
        free(current);
    }
    else {
        //printf("Insert IO\n");
        insertIOq(current);
    }
}

void makeNode(int pr, int cibs, char *cpuio) {
    struct PCB_st *pcb = malloc(sizeof(struct PCB_st));
    if(Head == NULL && Tail == NULL) {
        Head = pcb;
        Tail = pcb;
    }
    int sizecpub = ((cibs+1)/2);
    int sizeiob = ((cibs)/2);
    pcb->PR = pr;
    int *cpuburst = malloc(sizeof(int)*sizecpub);
    int *ioburst = malloc(sizeof(int)*sizecpub);
    pcb->numCPUB = sizecpub;
    pcb->numIOB = sizeiob;
    int i,j=0,k=0;
    //printf("%d makenode numIOB\n", pcb->numIOB); 
    //printf("%d cibs:  ",(cibs+1)/2);
    for(i=0; i<cibs; i++) {
        if (i % 2 == 0) {
            cpuburst[j++] = atoi(strtok_r(cpuio, " ", &cpuio));
            //printf("%d cpu: ",cpuburst[i]);
        }
        else { 
            ioburst[k++] = atoi(strtok_r(cpuio, " ", &cpuio));
            //printf("%d io: ", ioburst[i]);
        }
    }
    pcb->readyQ = 0;
    pcb->ioQ = 0;
    pcb->enterQ = 0;
    pcb->enterioQ =0;
    pcb->CPUBurst = cpuburst;
    pcb->IOBurst = ioburst;
    pcb->cpuindex = 0;
    pcb->ioindex = 0;
    //printf("Finished making node mani dlist now\n");
    if(Head == NULL) setHead();
    if(Tail == NULL) setTail();
    push(pcb);
}

void insertIOq(struct PCB_st *p) {
    if(IOHead == p)
        return;
    struct PCB_st *current = IOHead;
    struct PCB_st *previous = IOTail;
    if(IOHead == NULL && IOTail == NULL) {
        //printf("Empty IO Q\n");
        IOHead = p;
        IOTail = p;
        IOHead->prev = NULL;
        IOHead->next = NULL;
        //printf("No longer empt\n");
        return;
    }
    //printf("Inserting again!\n");
    while(current->next != NULL) {
        current = current->next;
    }
    current->next = p;
    IOTail = p;
    IOTail->prev = previous;
    previous = NULL;
    current = NULL;
    //printf("IO push finish\n");
    return;
}

void push(struct PCB_st *p) {
    if(Head == p){
        //printf("Head is eql to cur\n");
        return;
    }
    //printf("IN push step one\n");
    //if (Head == NULL) printf("Head is null Boo boo\n");
    //if (Tail == NULL) printf("Tail is null BOoo boo\n");
    struct PCB_st *current = Head;
    struct PCB_st *previous = Tail;
    //printf("In push step 2\n");
    while(current->next != NULL) {
        //printf("before assignment THIS SHOULD DO SOMETHINGAAAAAAAAAAAAAAAAAAAAAAAA!\n");
        current = current->next;
        //printf("after assignment\n");
    }
    //printf("IN PUSH STEP 3\n");
    
    current->next = p;
    Tail = p;
    Tail->prev = previous;
    Tail->enterQ = clock();
    previous = NULL;
    current = NULL;
    return;
}

void parseArg(int argc, char* argv[]) {
    int i;
    for (i=0; i<argc; i++) {
        if(strcmp(argv[i], "-input") == 0)
            input = argv[i+1];
        else if (strcmp(argv[i], "-alg") == 0)
            algo = argv[i+1];
        else
            continue;
    }
}

void setHead() {
    struct PCB_st *current = Tail;
    while (current->prev != NULL) {
        current = current->prev;
    }
    Head = current;
    //current = NULL;
}

void setTail() {
    struct PCB_st *current = Head;
    while (current->next != NULL) {
        current = current->next;
    }
    Tail = current;
    //current = NULL;
}

void printarr(int *arr) {
    int size = SZ(arr);//sizeof(arr)/(sizeof(arrgg)/sizeof(int));
    printf("%d:", size);
    int i;
    for(i=0; i<size; i++) {
        printf("element %d: %d\n", i, arr[i]);
    }
}

void testThread() {
    struct PCB_st *current =Head;
    while (current->next != NULL) {
        printf("%d PR\n", current->PR);
        printarr(current->CPUBurst);
        printarr(current->IOBurst);
        current = current->next;
    }
    printf("Done\n");
}

void printResults(double dur) {
    double throu = (double)(Total_job)/CLOCK;
    double util = (double)(cpu_util)/CLOCK;
    printf("-------------------------------------------------------\n");
    printf("Input File Name \t\t: %s\n", input);
    printf("CPU Scheduling Alg \t\t: %s\n", algo);
    printf("CPU utilization\t\t\t: %.2f\n", util*100);
    printf("Throughput\t\t\t: %.3f processes/ms\n",throu );
    printf("Avg. Turnaround time\t\t: %d\n", CLOCK );
    printf("Avg. Waiting time in R queue\t: %.3fms\n", (cpurq)/CLOCKS_PER_SEC);
}

int main(int argc, char* argv[]) {
    double duration;
    clock_t start_t, end_t;
    parseArg(argc, argv);
    sem_init(&sem_cpu, 0, 0);
    sem_init(&sem_io, 0, 0);
    start_t = clock();
    pthread_mutex_init(&lock, NULL);
    pthread_t fReadth, cpuscheduleth, iosysth;

    pthread_create(&cpuscheduleth, NULL, CPU_scheduler_thread, NULL);
    pthread_create(&fReadth, NULL, FileRead_Thread, NULL);
    pthread_create(&iosysth, NULL, IO_system_thread, NULL);

    pthread_join(fReadth, NULL);
    pthread_join(cpuscheduleth, NULL);
    pthread_join(iosysth, NULL);
    end_t = clock();
    duration = (double)(end_t - start_t) / CLOCKS_PER_SEC;
    printResults(duration);
    //testThread();

    return 0;
}
