#ifndef _VITERBI_DECODER_H_
#define _VITERBI_DECODER_H_

#include "convCodeParams.h"
#include "convHelpers.h"
#include <stdbool.h>

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
 * @brief Performs hard decision viterbi decoding of the specified code.
 * 
 * @note The code is expected to begin in the specified beginning state and end in the 0 state.
 *       Ending in the 
 * 
 * @param codedSegments an array of k bit codedSegments.  Each segment is in a seperate byte.  BLOCK_SIZE k-bit segements are passed at a time.
 * @param uncoded an array of uncoded bytes.  It is asumed the transmission is in big endian order.  Within a k bit segment, it is assumed it was sent in big endian order so simply appending the index (uncoded bits) corresponding to the edge can simply be shifted onto the word.  If the coded message is not a multiple of the block size, fill the block with 0's.
 * @param segmentsIn The number of coded segements being provided, which may be less than the block length.
 * @param last If true, returns the remaining traceback and resets after this iteration
 * @returns The number of uncoded bytes returned
 */ 
int viterbiDecoderHard(viterbiHardState_t* state, uint8_t* codedSegments, uint8_t* uncoded, int segmentsIn, bool last);

/**
 * @brief Swaps the node metric and traceback arrays.  Used to update both the node metrics and traceback arrays after a trellis iteration.
 * 
 * Using 2 buffers allows us to avoid copy operations from an intermediate array
 */
void swapViterbiArrays(viterbiHardState_t* state);

int viterbiConfigCheck();

void viterbiInit(viterbiHardState_t* state);

void resetViterbiDecoderHard(viterbiHardState_t* state);

uint8_t calcHammingDist(uint8_t a, uint8_t b);

int argminPathMetrics(METRIC_TYPE *metrics);

int argminNodeMetrics(METRIC_TYPE *metrics);

#endif