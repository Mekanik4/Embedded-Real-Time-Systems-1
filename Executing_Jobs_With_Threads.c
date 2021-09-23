#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#define QUEUESIZE 10
#define LOOP 200
#define P 5
#define Q 5
int latency[LOOP];
int counter = 0;
int elementsLeft = LOOP;

struct timeval tic(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

double toc(struct timeval begin){
    struct timeval end;
    gettimeofday(&end, NULL);
    double stime = ((double)(end.tv_sec - begin.tv_sec) * 1000) +
                   ((double)(end.tv_usec - begin.tv_usec) / 1000);
    stime = stime / 1000;
    return (stime);
}

void writeFile(int *a, int elements, int pro, int con){
    FILE *fp;
    char *name = (char *)malloc(6 * sizeof(char));
    sprintf(name, "times_%d Pro_%d Con_%d E.csv", pro, con, elements);
    fp = fopen(name, "w"); 
    for (int i = 0; i < elements; i++){
        fprintf(fp, "%d", a[i]); 
        fprintf(fp, "\n");
    }
}

void *producer(void *args);
void *consumer(void *args);

void *function(void *arg){
    
    int b = *((int *)arg);
    float result=0;
    for(int i=0;i<1000;i++){
        if(i%2==0){
            result+=b + i*cos(b);
        }
        else{
            result-=b + i*sin(b);
        }
    }
    printf("%d\n",b);
}

typedef struct{ 
    struct timeval tv;
    int arguement;
   
} element;

typedef struct {
    void *(*work)(void *);
    void *arg;
} workFunction;

typedef struct
{
    workFunction *buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit(void);
void queueDelete(queue *q);
void queueAdd(queue *q, workFunction *in);
void queueDel(queue *q, workFunction *out);

int main()
{
    queue *fifo;
    pthread_t pro[P], con[Q];

    fifo = queueInit();
    if (fifo == NULL)
    {
        fprintf(stderr, "main: Queue Init failed.\n");
        exit(1);
    }

    for (int i = 0; i < P; i++)
        pthread_create(&pro[i], NULL, producer, fifo);
    for (int i = 0; i < Q; i++)
        pthread_create(&con[i], NULL, consumer, fifo);
    for (int i = 0; i < P; i++)
        pthread_join(pro[i], NULL);
    for (int i = 0; i < Q; i++)
        pthread_join(con[i], NULL);
    queueDelete(fifo);
    float mean=0;
    for (int i = 0; i < LOOP; i++){
        //printf("%d:  %d ms\n", (i+1), latency[i]);
        mean+=latency[i];
    }
    printf("%d\n", (int)(mean/LOOP));
    //writeFile(latency, LOOP, P, Q );

    return 0;
}


void *producer(void *q)
{
    queue *fifo;
    int i;
    int *a = (int *)malloc(LOOP * sizeof(int));
    for (i = 0; i < LOOP; i++){
         a[i] = i;
    }

    workFunction *po;
    po=(workFunction*)malloc(LOOP * sizeof (workFunction));

    element *el;
    el=(element *)malloc(LOOP * sizeof(element));

    fifo = (queue *)q;

    for (i = 0; counter < LOOP; i++)
    {
        (el+i)->tv=tic();
        (el+i)->arguement = a[counter];
        (po+i)->arg = (el+i);
        (po+i)->work=function;
       
        pthread_mutex_lock(fifo->mut);
        
        while (fifo->full)
        {
            //printf("producer: queue FULL.\n");
            pthread_cond_wait(fifo->notFull, fifo->mut);
        }

        queueAdd(fifo, (po+i));
        counter++;
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
        
    }
    return (NULL);
}

void *consumer(void *q)
{
    queue *fifo;
    fifo = (queue *)q;

    while (elementsLeft > (Q-1))
    {
        pthread_mutex_lock(fifo->mut);
        while (fifo->empty)
        {
            //printf("consumer: queue EMPTY.\n");
            pthread_cond_wait(fifo->notEmpty, fifo->mut);
            if(elementsLeft<0){
                exit(0);
            }
        }
       
        workFunction d;
        element *e;
        
        queueDel(fifo, &d);
        elementsLeft--;
        e=d.arg;
        struct timeval start = e->tv;
        double final = toc(start);
        int argmnt = e->arguement;
        printf("Consumer: ");
        (*d.work)(&argmnt);  

        latency[(LOOP-elementsLeft-1)] = (int)(final*(1000000));
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notFull);
    }

    return (NULL);
}

queue *queueInit(void)
{
    queue *q;

    q = (queue *)malloc(sizeof(queue));
    if (q == NULL)
        return (NULL);

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mut, NULL);
    q->notFull = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notEmpty, NULL);

    return (q);
}

void queueDelete(queue *q)
{
    pthread_mutex_destroy(q->mut);
    free(q->mut);
    pthread_cond_destroy(q->notFull);
    free(q->notFull);
    pthread_cond_destroy(q->notEmpty);
    free(q->notEmpty);
    free(q);
}

void queueAdd(queue *q, workFunction *in)
{
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

void queueDel(queue *q, workFunction *out)
{
    *out = *q->buf[q->head];
    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}

