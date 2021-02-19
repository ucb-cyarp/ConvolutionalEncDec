#ifndef _GNU_SOURCE
//Need _GNU_SOURCE, sched.h, and unistd.h for setting thread affinity in Linux
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <pthread.h>

#include "convEncode.h"
#include "viterbiDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#define ENCODE_PKT_BYTE_LEN (1024)
#define PKTS (16)
#define PRINT_INTERVAL (1)
#define PRINT_CHECK_INTERVAL (1000)

#define CPU (4)

//From telemetry_helpers.c
typedef struct timespec timespec_t;
double difftimespec(timespec_t* a, timespec_t* b){
    double a_double = a->tv_sec + (a->tv_nsec)*(0.000000001);
    double b_double = b->tv_sec + (b->tv_nsec)*(0.000000001);
    return a_double - b_double;
}
double timespecToDouble(timespec_t* a){
    double a_double = a->tv_sec + (a->tv_nsec)*(0.000000001);
    return a_double;
}

void* testThread(void* arg){
    srand(314);

    //TOOD: Form a set of random packets to send
    uint8_t uncodedPkts[PKTS][ENCODE_PKT_BYTE_LEN];

    for(int i = 0; i<PKTS; i++){
        for(int j = 0; j<ENCODE_PKT_BYTE_LEN; j++){
            uncodedPkts[i][j] = (uint8_t) rand();
        }
    }

    //Initialize the Encoder
    convEncoderState_t convEncState;
    resetConvEncoder(&convEncState);
    initConvEncoder(&convEncState);

    //Encode the packets
    int64_t bytesEncoded = 0;
    uint8_t codedSegments[8*ENCODE_PKT_BYTE_LEN/k+S];
    int currentPkt = 0;

    timespec_t startTime;
    asm volatile ("" ::: "memory"); //Stop Re-ordering of timer
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    asm volatile ("" ::: "memory"); //Stop Re-ordering of timer
    timespec_t lastPrint = startTime;
    int printCheck = 0;
    while(1){

        int codedSegsReturned = convEnc(&convEncState, uncodedPkts[currentPkt], codedSegments, ENCODE_PKT_BYTE_LEN, true);
        if(currentPkt<(PKTS-1)){
            currentPkt++;
        }else{
            currentPkt = 0;
        }
        bytesEncoded+=ENCODE_PKT_BYTE_LEN;

        // //TODO: Remove
        // assert(codedSegsReturned == 8*ENCODE_PKT_BYTE_LEN+S);

        //Need to make sure that the encode is not optimized out
        asm volatile(""
        :
        : "r" (*(const uint8_t (*)[]) codedSegments) //See https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html for information for "string memory arguments"
        :);

        if(printCheck >= PRINT_CHECK_INTERVAL){
            timespec_t currentTime;
            asm volatile ("" ::: "memory"); //Stop Re-ordering of timer
            clock_gettime(CLOCK_MONOTONIC, &currentTime);
            asm volatile ("" ::: "memory"); //Stop Re-ordering of timer
            double duration = difftimespec(&currentTime, &lastPrint);
            if(duration >= PRINT_INTERVAL){
                double rateDurringPeriod = bytesEncoded*8 / duration / 1e6;
                printf("Encoded %ld bits in %f Seconds, Rate: %f Mbps\n", bytesEncoded*8, duration, rateDurringPeriod);
                bytesEncoded = 0;

                asm volatile ("" ::: "memory"); //Stop Re-ordering of timer
                clock_gettime(CLOCK_MONOTONIC, &lastPrint);
                asm volatile ("" ::: "memory"); //Stop Re-ordering of timer
            }

            printCheck=0;
        }else{
            printCheck++;
        }
    }

    return NULL;
}

int main(int argc, char* argv[]){
    printf("Params:\n");
    printf("\tk:    %d\n", k);
    printf("\tK:    %d\n", K);
    printf("\tn:    %d\n", n);
    for(int i = 0; i<n; i++){
        printf("\t\tg[%d]=%lo\n", i, g[i]);
    }
    printf("\tRate: %f\n", Rc);
    printf("\tNum States: %lu\n", NUM_STATES);

    //Create Thread Parameters
    int status;
    pthread_t thread;
    pthread_attr_t attr;

    status = pthread_attr_init(&attr);
    if(status != 0)
    {
        printf("Could not create pthread attributes ... exiting");
        exit(1);
    }

    //Set partition to run on
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset); //Clear cpuset
    CPU_SET(CPU, &cpuset); //Add CPU to cpuset
    status = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);//Set thread CPU affinity
    if(status != 0)
    {
        printf("Could not set thread core affinity ... exiting");
        exit(1);
    }

    //Start Threads
    status = pthread_create(&thread, &attr, testThread, NULL);
    if(status != 0)
    {
        printf("Could not create a thread ... exiting");
        errno = status;
        perror(NULL);
        exit(1);
    }

    //Wait for Thread to Finish
    void *res;
    status = pthread_join(thread, &res);
    if(status != 0)
    {
        printf("Could not join a thread ... exiting");
        errno = status;
        perror(NULL);
        exit(1);
    }
}