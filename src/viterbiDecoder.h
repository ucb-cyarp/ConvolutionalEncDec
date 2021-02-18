#ifndef _VITERBI_DECODER_H_
#define _VITERBI_DECODER_H_

#include "convCodePerams.h"
#include "convHelpers.h"

//Note, this is being written in pure C to match
//the output of Laminar and to avoid any extra C++
//logic.  Unfortunatly, that means we don't have access
//to templates.
//To sidestep this, we will define the code properties in
//another file which is included.  Only one instance is
//supported at a time using this method but some external
//scripting could be used to generate multiple discriptions

#define NUM_STATES (POW2(k*S))

#define METRIC_TYPE uint32_t
#define TRACEBACK_TYPE uint64_t

#define EDGE_METRIC_INDEX_TYPE uint8_t //This defines the index of the comparison to be used when evaluating edges.  For 2 codes bits, there are 4 possibilities, 00, 01, 10, 11.

#define TRACEBACK_LEN (5*K)

/**
 * State for the viterbi decoder between calls
 * 
 */
typedef struct{
    //Code Configuration
    EDGE_METRIC_INDEX_TYPE edgeCodedBits[NUM_STATES][POW2(k)];
    //This contains the coded bits corresponding to the given endge.
    //There is one entry for each edge with the ith edge of the jth node having index
    //j*2^k+i in this array.
    //The index stored points to the metric of the coded bits corresponding to the particular branch.
    //For n=2, there are 4 possible coded bit sequences corresponding to 00, 01, 10, 11
    //The hamming distance will only be computed once per possible coded bit segments
    //The codes bits can be used as an index into the computed hamming distances.
    //The edge index represents the coded bits to append to the traceback

    //Decoder State
    //Using 2 arrays which will be swapped in each itteration
    //Allows us to write to a des
    METRIC_TYPE nodeMetricsA[NUM_STATES];
    TRACEBACK_TYPE traceBackA[NUM_STATES];
    METRIC_TYPE nodeMetricsB[NUM_STATES];
    TRACEBACK_TYPE traceBackB[NUM_STATES];

    METRIC_TYPE* nodeMetricsCur;
    TRACEBACK_TYPE* traceBackCur;
    METRIC_TYPE* nodeMetricsNext;
    TRACEBACK_TYPE* traceBackNext;

    int iteration; //Used to track when to start making traceback decisions
    uint8_t decodeCarryOver;
    uint8_t decodeCarryOverCount;
} viterbiHardState_t;

/**
 * @brief Swaps the node metric and traceback arrays.  Used to update both the node metrics and traceback arrays after a trellis iteration.
 * 
 * Using 2 buffers allows us to avoid copy operations from an intermediate array
 */
inline void swapViterbiArrays(viterbiHardState_t* state);

int viterbiConfigCheck();

void viterbiInit(viterbiHardState_t* state);

void resetViterbiDecoderHard(viterbiHardState_t* state);

inline uint8_t calcHammingDist(uint8_t a, uint8_t b);

inline int argminPathMetrics(METRIC_TYPE *metrics);

inline int argminNodeMetrics(METRIC_TYPE *metrics);

#endif