
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//each row from input is saved as a row
typedef struct {
    int pid;
    int arrival;
    int first_resp; 
    int burst;
    int index;      
} Row;

//dynamic array to save the rows 
typedef struct {
    Row *data;
    size_t size;
    size_t cap;
} Rows;

//init an empty array to start
static void rows_init(Rows *rs){
    rs->data=NULL;
    rs->size=0;
    rs->cap=0;
}

//read each row, then push each row into the array
static void rows_push(Rows *rs, Row r){

    if(rs->size==rs->cap){

        size_t ncap = rs->cap? rs->cap*2 : 256;
        Row *tmp = realloc(rs->data, ncap*sizeof(Row));

        if(!tmp){
            fprintf(stderr,"out of memory\n");
            exit(1);
        }

        rs->data=tmp;
        rs->cap=ncap;
    }
    //store and grow array
    rs->data[rs->size++]=r; 
}

//function to break tie when processes arrive at the same same time
static int cmp_row(const void *a, const void *b){

    const Row *ra=(const Row*)a, *rb=(const Row*)b;

    if(ra->arrival!=rb->arrival){
    return ra->arrival<rb->arrival? -1:1;
    }

    if(ra->pid!=rb->pid){
    return ra->pid<rb->pid? -1:1;
    }
    //if niether
    return ra->index<rb->index? -1 : (ra->index>rb->index);
}

static void simulate_and_write(const Row *arr, size_t n, int latency, FILE *f_details, FILE *f_summary){

    long long current_time=0;
    long long total_wait=0, total_turn=0, total_resp=0;
    int first_arrival = arr[0].arrival;
    long long last_finish=0;

    //Simulate each job in arrival+pid order
    for(size_t i=0;i<n;i++){
        const Row *p=&arr[i];

        //CPU idle until the job  available
        if(current_time<p->arrival) {
            current_time=p->arrival;
        }
        //dispatcher overhead before starting simulation
        long long start = current_time + latency;
        long long finish = start + p->burst;
        long long turnaround = finish - p->arrival;
        long long waiting = start - p->arrival;
        long long response = waiting + p->first_resp; // FCFS property

        total_turn += turnaround;
        total_wait += waiting;
        total_resp += response;

        //write simulated values
        fprintf(f_details,"%d,%d,%d,%lld,%lld,%lld,%lld,%lld\n",
                latency, p->pid, p->arrival, start, finish, turnaround, waiting, response);

        current_time=finish;
        last_finish=finish;
    }

    //find averages by dividing by job count
    double dn = n;
    double avg_wait= total_wait/dn;
    double avg_turn= total_turn/dn;
    double avg_resp=total_resp/dn;
    double elapsed = last_finish - first_arrival;
    double throughput = dn/elapsed;

    fprintf(f_summary,"%d,%.6f,%.2f,%.2f,%.2f\n", latency, throughput, avg_wait, avg_turn, avg_resp);
}

int main(void){

    //read header from stdin
    char buf[1024];

    if(!fgets(buf,sizeof(buf),stdin)){
        fprintf(stderr,"no input (expected header)\n");
        return 1;
    }

    Rows rs; rows_init(&rs);
    size_t line_no=1;

    while(fgets(buf,sizeof(buf),stdin)){

        line_no++;
        // skip blank and comment lines
        int blank=1;

        for(char *p=buf;*p;p++){ 
            if(*p!=' '&&*p!='\t'&&*p!='\n'&&*p!='\r'){ blank=0;break; } 
        }

        if(blank) continue;
        if(buf[0]=='#') continue;

        //read rows
        Row r; r.index=(int)rs.size;
        int ok = sscanf(buf," %d , %d , %d , %d ",&r.pid,&r.arrival,&r.first_resp,&r.burst);

        rows_push(&rs,r);
    }

    //sort once by arrival, pid to enforce FCFS + tie-break
    qsort(rs.data, rs.size, sizeof(Row), cmp_row);

    //open output files to write to
    FILE *f_details = fopen("fcfs_results_details.csv","w");
    //error check
    if(!f_details){ 
        fprintf(stderr,"cannot open fcfs_results_details.csv for write\n"); free(rs.data); 
        return 1; 
    }

    FILE *f_summary = fopen("fcfs_results.csv","w");
    //error check
    if(!f_summary){ 
        fprintf(stderr,"cannot open fcfs_results.csv for write\n"); fclose(f_details); free(rs.data); 
        return 1; 
    }

    //write headers in files 
    fprintf(f_details,"latency,pid,arrival,start,finish,turnaround,waiting,response\n");
    
    fprintf(f_summary,"latency,throughput,avg_wait,avg_turnaround,avg_response\n");

    //simulate latency 
    for(int L=1; L<=200; L++){
        simulate_and_write(rs.data, rs.size, L, f_details, f_summary);
    }

    printf("RR simulation completed! Results saved to fcfs_results.csv");
    printf("Average results saved to fcfs_results_details.csv");
    
    fclose(f_details);
    fclose(f_summary);
    free(rs.data);

    return 0;
}
