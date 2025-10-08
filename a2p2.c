//a2p2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//process has 4 values
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

//init list with 0
static void list_init(ProcList *pl){
    pl->data = NULL;
    pl->size = 0;
    pl->cap = 0;
}

//add process to the list, and gorw the array
static void list_push(ProcList *pl, Proc p){

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

//simulate round robin
static void simulate_rr(const Proc *p, size_t n, int quantum, int latency, FILE *f_details, FILE *f_summary){

    if (n == 0) return;

    int *rem = malloc(sizeof(int) * n);
    int *first_start = malloc(sizeof(int) * n);
    int *finish = malloc(sizeof(int) * n);

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
    int time = 0;
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
            q_push(&rq, i);
        } 
        //if the process is finished
        else {
            finish[i] = time;
            last_finish = time;

            //calculate values
            int turnaround = finish[i] - p[i].arrival;
            int waiting = turnaround - p[i].burst;
            int response = (first_start[i] - p[i].arrival) + p[i].first_resp;
            
            //write values to file
            fprintf(f_details, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", latency, quantum, p[i].pid, p[i].arrival, first_start[i], finish[i], 
                turnaround, waiting, response);

            done++;
        }
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
    double dn = n;
    double avg_wait = sum_wait/dn;
    double avg_turn = sum_turn/dn;
    double avg_resp = sum_resp/dn;
    double throughput = last_finish - first_arrival;

    fprintf(f_summary, "%d,%.6f,%.2f,%.2f,%.2f\n", quantum, throughput, avg_wait, avg_turn, avg_resp);

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
    list_init(&pl);

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
        list_push(&pl, pr);
    }

    // Sort by arrival then PID
    qsort(pl.data, pl.size, sizeof(Proc), cmp_proc);

    // Open outputs to write to
    FILE *f_details = fopen("rr_results_details.csv", "w");
    FILE *f_summary = fopen("rr_results.csv", "w");


    // CSV headers
    fprintf(f_details, "latency,quantum,pid,arrival,start,finish,turnaround,waiting,response\n");
    fprintf(f_summary, "quantum,throughput,avg_wait,avg_turnaround,avg_response\n");

    // Assignment Part II: sweep quantum 1..200, latency fixed at 20
    const int latency = 20;
    for (int q = 1; q <= 200; q++) {
        simulate_rr(pl.data, pl.size, q, latency, f_details, f_summary);
    }

    fclose(f_details);
    fclose(f_summary);
    free(pl.data);
    return 0;
}
