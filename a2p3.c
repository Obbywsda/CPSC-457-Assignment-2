#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
    double arrival;
    double finish_time;
    double waiting_time;
    double turnaround_time;  
    double response_time;
} dataValue;

typedef struct{
    int pid;
    int arrival;
    int first_resp;  
    int burst;
} Proc;
//process list

typedef struct{
    Proc *data;
    size_t size;
    size_t cap;
} ProcList;

typedef struct{
    dataValue *data;
    size_t size;
    size_t cap;
} DataList;

typedef struct{
    ProcList *unfinishedProc;
    DataList *unfinishedData;
    DataList *finishedData;
    int totalTime;
} queues;

//init list with 0
static void proc_list_init(ProcList *pl){
    pl->data = NULL;
    pl->size = 0;
    pl->cap = 0;
}

static void data_list_init(DataList *pl){
    pl->data = NULL;
    pl->size = 0;
    pl->cap = 0;
}


//add process to the list, and grow the array
static void proc_list_push(ProcList *pl, Proc p){

    if (pl->size == pl->cap){
        size_t nc = pl->cap ? pl->cap * 2 : 256;
        Proc *tmp = (Proc*)realloc(pl->data, nc * sizeof(Proc));

        if (!tmp) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        pl->data = tmp;
        pl->cap = nc;
    }
    pl->data[pl->size++] = p;
}

static void data_list_push(DataList *pl, dataValue p){

    if (pl->size == pl->cap){
        size_t nc = pl->cap ? pl->cap * 2 : 256;
        dataValue *tmp = (dataValue*)realloc(pl->data, nc * sizeof(dataValue));

        if (!tmp) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        pl->data = tmp;
        pl->cap = nc;
    }
    pl->data[pl->size++] = p;
}

//sort by arrival then PID for tiebreak
static int cmp_proc(const void *a, const void *b){

    const Proc *x = (const Proc*)a, *y = (const Proc*)b;

    if (x->arrival != y->arrival){
        return (x->arrival < y->arrival) ? -1 : 1;
    }
    if (x->pid != y->pid){
        return (x->pid < y->pid)     ? -1 : 1;
    }
    return 0;
}

//queue of ints that indexes into process array
typedef struct{
    int *buf;
    int head;
    int tail;
    int cap;
} procQueue;

//initialize the queue
static void q_init(procQueue *q, int cap){

    if (cap < 4){
        cap = 4;
    }

    q->buf = malloc(sizeof(int) * cap);

    if (!q->buf){
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    q->head = 0;
    q->tail = 0;
    q->cap  = cap;
}

//check to see if the queue is empty
static int q_empty(const procQueue  *q){

    return q->head == q->tail;
}


static void q_grow(procQueue *q){

    int size = q->tail - q->head;                  
    int newcap = q->cap * 2;                       
    int *nb = malloc(sizeof(int) * newcap);

    if (!nb){
        fprintf(stderr, "out of memory\n"); exit(1); 
    }

    if (size > 0){
        memcpy(nb, q->buf + q->head, sizeof(int) * size);
    }

    free(q->buf);
    q->buf = nb;
    q->cap = newcap;
    q->head = 0;                                   
    q->tail = size;                                
}

//push to the back of the queue
static void q_push(procQueue *q, int v){
    if (q->tail == q->cap) {
        //if the buffer is full, grow 
        q_grow(q);
    }
    q->buf[q->tail++] = v;
}

static int q_pop(procQueue *q){

    if (q_empty(q)){
        fprintf(stderr, "queue underflow\n"); exit(1); 
    }
    
    int v = q->buf[q->head++];

    if (q->head == q->tail){                      
        q->head = q->tail = 0;
    }
    return v;
}

static void q_free(procQueue *q) {
    free(q->buf);
    q->buf = NULL;
    q->cap = q->head = q->tail = 0;
}

static queues simulate_rr(const Proc* p, size_t n, int quantum, int latency, const dataValue* a, int time){

    if (n == 0) return;

    int *rem = malloc(sizeof(int) * n);
    int *first_start = malloc(sizeof(int) * n);
    int *finish = malloc(sizeof(int) * n);
    ProcList pl1;
    proc_list_init(&pl1);

    //check to see if there is enouogh memory
    if (!rem || !first_start || !finish){
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    //initialize arrays
    for (size_t i = 0; i < n; i++){

        //remaining for process is the full burst time
        rem[i] = p[i].burst;
        first_start[i] = -1;
        finish[i] = -1;
    }

    //create procQueue and allocate space
    procQueue rq;
    q_init(&rq, (int)n * 2 + 4);

    //rr simulation clock
    //index of the next process set by arrival time
    size_t next_arr = 0;
    //how many process are done
    int done = 0;

    //start clock at first arrival time
    if (next_arr < n){
        time = p[next_arr].arrival;
    }
    while (next_arr < n && p[next_arr].arrival <= time){
        q_push(&rq, (int)next_arr);
        next_arr++;
    }

    //check arrival time to calculate thruput
    int first_arrival = p[0].arrival;
    int last_finish = time;

    //loop through the queue
    while (done < (int)n){
        //check to see if a process is ready
        if (q_empty(&rq)){
            //if another process arrive before an existing one can start, start with the next process
            if (next_arr < n){
                time = p[next_arr].arrival;
                //start whatever is available
                while (next_arr < n && p[next_arr].arrival <= time){
                    //add to the ready queue
                    q_push(&rq, (int)next_arr);
                    next_arr++;
                }
                continue;
            } 
            else{
                break;
            }
        }

        //start next process
        int i = q_pop(&rq);

        //account for latency
        time += latency;

        if (first_start[i] == -1){
            first_start[i] = time;
        }

        //calculaate slice length, and reduce the remaining time by how much has already been processed
        int run = (rem[i] < quantum) ? rem[i] : quantum;
        time += run;
        rem[i] -= run;

        //if there is any new processes that have arrived, add them to the queue
        while (next_arr < n && p[next_arr].arrival <= time){
            q_push(&rq, (int)next_arr);
            next_arr++;
        }

        //check to see if the current process still has work left, if it does then add it back to te queue
        if (rem[i] > 0){
            int pass = q_pop(&rq);
            proc_list_push(&pl1,p[pass]);
            pl1.data[sizeof(pl1.data)].arrival = 0;
            pl1.data[sizeof(pl1.data)].burst = rem[i];
            int response = (first_start[i] - p[i].arrival) + p[i].first_resp;
            dataValue temp;
            temp.arrival = p[i].arrival;
            temp.finish_time += finish[i];
            temp.response_time = response;
        } 
        //if the process is finished
        else {
            finish[i] = time;
            last_finish = time;

            //calculate values
            int turnaround = finish[i] - p[i].arrival;
            int waiting = turnaround - p[i].burst;
            int response = (first_start[i] - p[i].arrival) + p[i].first_resp;
            dataValue temp;
            temp.arrival = p[i].arrival;
            temp.finish_time += finish[i];
            temp.response_time = response;
            temp.turnaround_time = turnaround;
            temp.waiting_time = waiting;

            
        }
        done++;
    }

    double sum_wait = 0.0;
    double sum_turn = 0.0;
    double sum_resp = 0.0;

    //loop to sum up wait times, turnaround times and response times
    for (size_t i = 0; i < n; i++){

        int turnaround = finish[i] - p[i].arrival;
        int waiting = turnaround - p[i].burst;
        int response = (first_start[i] - p[i].arrival) + p[i].first_resp;

        sum_turn += turnaround;
        sum_wait += waiting;
        sum_resp += response;
    }

    //sum divided by number of jobs

    q_free(&rq);
    free(rem);
    free(first_start);
    free(finish);
}


int main(void){
    //read and ignore the first line
    char line[1024];
    if (!fgets(line, sizeof(line), stdin)){
        fprintf(stderr, "no input (expected header)\n");
        return 1;
    }

    //read every row from input

    //create list
    ProcList pl;
    proc_list_init(&pl);

    while (fgets(line, sizeof(line), stdin)){

        //create variables to assign input values to
        int pid;
        int arr;
        int first_resp;
        int burst;

        //read and assign values
        sscanf(line, " %d , %d , %d , %d ", &pid, &arr, &first_resp, &burst);

        //create process and assign values
        Proc pr;
        pr.pid = pid;
        pr.arrival = arr;
        pr.first_resp = first_resp;
        pr.burst = burst;

        //add it to the queue
        proc_list_push(&pl, pr);
    }
    
    const int latency = 20;
    queues ness;
    DataList unfinished;
    data_list_init(&unfinished);
    ness.unfinishedData = &unfinished;
    simulate_rr(pl.data, pl.size, 40, latency, unfinished.data, 0);
    

    return 0;
}