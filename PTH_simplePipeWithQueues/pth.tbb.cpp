// a very simple three stage pipeline with queues in between stages. 
// the first stage has just an output queue. the last stage has both input and
// output queues, and the output queue of the last stage is written to a result file.
// queues are also bounded and the total amount of data that flows around is larger than
// the capacity of queues. 
//
//     gcc -o simplepipewithqueues -lpthread original.c
//     ./original
//  

#define _reentrant
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <tbb/tbb.h>
#include <tbb/task_group.h>
#include <tbb/parallel_pipeline.h>

#define bufsize 1000//0
#define maxdata 5000//0
#define nrstages 3

#define ntoken 5

/* structure to keep info about queues between stages */
typedef struct {
    int* elements;
    int nr_elements; /* how many elements are currently in the queue */
    int capacity;
    int readfrom; /* position in the array from which to read from */
    int addto; /* position in the array of the first free slot for adding data */
} queue_t;

void add_to_queue(queue_t* queue, int elem)
{
    queue->elements[queue->addto] = elem;
    queue->addto = (queue->addto + 1) % queue->capacity;
    queue->nr_elements++;
}

int read_from_queue(queue_t* queue)
{
    int elem;
    elem = queue->elements[queue->readfrom];
    queue->nr_elements--;
    queue->readfrom = (queue->readfrom + 1) % queue->capacity;
    return elem;
}


/* structure to keep information about input and output queue of a pipeline stage */
typedef struct {
    queue_t* inputqueue;
    queue_t* outputqueue;
} pipeline_stage_queues_t;


void initialisequeue(queue_t* queue, int capacity) {
    queue->elements = (int*)malloc(sizeof(int) * capacity);
    queue->readfrom = 0;
    queue->addto = 0;
    queue->nr_elements = 0;
    queue->capacity = capacity;
}

struct s1pair {
    int i_1;
    int my_output_1;
    int addto;
    int nr_elements;
};

long long fib(int n)
{
    /* declare an array to store fibonacci numbers. */
    long long f[33];
    long long i;

    /* 0th and 1st number of the series are 0 and 1*/
    f[0] = 0;
    f[1] = 1;

    for (i = 2; i <= n; i++)
    {
        /* add the previous 2 numbers in the series
           and store it */
        f[i] = f[i - 1] + f[i - 2];
    }

    // printf("f[n]:%i\n",f[n]);
    return f[n];
}

long long fibr(long long n) {
    if (n <= 0) {
        return 0;
    }
    else if (n == 1) {
        return 1;
    }
    else {
        return (fibr(n - 1) + fibr(n - 2));
    }
}

struct pipestruct {
    // inputs
    int* i_1;
    int  capacity; // of the (exploded) final output queue; it's never updated...
    // outputs
    int* elements; // final output queue (exploded)
    int  addto;
    int  nr_elements;
};

pipestruct pipetbb(pipestruct arg) {
    tbb::detail::d1::parallel_pipeline( /*max_number_of_live_token=*/ 2,
        tbb::detail::d1::make_filter<void, int>(
            tbb::detail::d1::filter_mode::serial_in_order,
                [&](tbb::flow_control& fc)-> int {
                // printf("hello\n");
                if (*(arg.i_1) >= 0) {
                    // printf("i_1: %i\n",*i_1);
                    int my_output = *(arg.i_1);
                    *(arg.i_1) = *(arg.i_1) - 1;
                    return my_output;
                }
                else {
                    // printf("stop\n");
                    fc.stop();
                    return 0;
                }
            }
            ) &
        tbb::detail::d1::make_filter<int, int>(

            tbb::detail::d1::filter_mode::parallel,
            [](int my_input) {
                int my_output;
            // printf("my_input : %i\n", my_input);
            if (my_input > 0) {
                long long fibn = fibr(32);
                // fprintf(stderr, "fib : %lld\n", fibn);
                my_output = my_input + 1;
            }
            else { // 0 is a terminating token...if we get it, we just pass it on...
                my_output = 0;
                // add_to_queue(myoutputqueue_2, my_output_2);
                // printf("my_output:%i\n",my_output);
            }
            return my_output;
            }
            ) &
                tbb::detail::d1::make_filter<int, void>(
                    tbb::detail::d1::filter_mode::serial_in_order,
                    [&](int my_input) {
                        int my_output;
            if (my_input >= 0)
                my_output = my_input * 2;
            else
                my_output = -1;
            // add_to_queue(myoutputqueue, my_output);
            arg.elements[arg.addto] = my_output;
            arg.addto = (arg.addto + 1) % arg.capacity;
            arg.nr_elements = arg.nr_elements + 1;
                    }
                    )
                );
    return arg;
}

void pipe(void* a1, void* a2, void* a3) {
    // stage one
    // int my_output_1, 
    int i_1 = maxdata;

    // pipeline_stage_queues_t *myqueues_1 = (pipeline_stage_queues_t *)a1;
    // queue_t *myoutputqueue_1 = myqueues_1->outputqueue;

    // // stage two
    // int my_input_2;
    // int my_output_2;

    // pipeline_stage_queues_t *myqueues_2 = (pipeline_stage_queues_t *)a2;
    // queue_t *myoutputqueue_2 = myqueues_2->outputqueue;
    // queue_t *myinputqueue_2 = myqueues_2->inputqueue;

    // // stage three
    // int my_input_3;
    // int my_output_3;

    pipeline_stage_queues_t* myqueues_3 = (pipeline_stage_queues_t*)a3;
    queue_t* myoutputqueue_3 = myqueues_3->outputqueue;
    // queue_t *myinputqueue_3 = myqueues_3->inputqueue;

    pipestruct arg = pipestruct{ &i_1,
                                 myoutputqueue_3->capacity,
                                 myoutputqueue_3->elements,
                                 myoutputqueue_3->addto,
                                 myoutputqueue_3->nr_elements };

    pipestruct r = pipetbb(arg);
    // no need to update i_1 because we passed it in as a pointer for the source stage.
    // capacity is not updated
    myoutputqueue_3->elements = r.elements;
    myoutputqueue_3->addto = r.addto;
    myoutputqueue_3->nr_elements = r.nr_elements;
}

int main(int argc, char* argv[]) {
    long i;
    FILE* results;

    /* initialize queues */
    queue_t queue[nrstages];
    for (i = 0; i < nrstages; i++) {
        int capacity;
        if (i < nrstages - 1)
            capacity = bufsize;
        else
            capacity = maxdata + 1; /* the output queue of the last stage has infinite capacity */
        initialisequeue(&queue[i], capacity);
    }

    /* set input and output queues for each stage */
    pipeline_stage_queues_t stage_queues[nrstages];
    for (i = 0; i < nrstages; i++) {
        if (i > 0)
            stage_queues[i].inputqueue = &queue[i - 1];
        else
            stage_queues[i].inputqueue = NULL;
        stage_queues[i].outputqueue = &queue[i];
    }

    pipe((void*)&stage_queues[0], (void*)&stage_queues[1], (void*)&stage_queues[2]);

    queue_t output_queue = queue[nrstages - 1];


    results = fopen("results", "w");
    fprintf(results, "number of stages:  %d\n",nrstages);
    for (i = 0; i < maxdata; i++) {
        fprintf(results, "%d ", output_queue.elements[i]);
    }
    fprintf(results, "\n");
    fclose(results);
    return 0;
}

