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

//***** Decoder Options *******
#define MAX_PKT_LEN_UNCODED_BITS (1024*16) //The max packet length in uncoded bits
#define TRACEBACK_LEN (5*K)
//***** End Options ******

#define NUM_STATES (POW2(k*S))

// #define FORCE_NO_POPCNT_DECODER
// #define SIMPLE_MIN

//TODO: Look into re-normalizing metrics.  Can we set an upper bound on the difference between nodes in the trellis?

#define MAX_EDGE_WEIGHT (n) //With hamming distance as the metric, the max difference occurs if all bits are different

#define MAX_PKT_LEN_SEGMENTS (MAX_PKT_LEN_UNCODED_BITS + S)

#if MAX_EDGE_WEIGHT*MAX_PKT_LEN_SEGMENTS <= POW2(8)
    #define METRIC_TYPE uint8_t
#elif MAX_EDGE_WEIGHT*MAX_PKT_LEN_SEGMENTS <= POW2(16)
    #define METRIC_TYPE uint16_t
#elif MAX_EDGE_WEIGHT*MAX_PKT_LEN_SEGMENTS <= POW2(32)
    #define METRIC_TYPE uint32_t
#else
    #define METRIC_TYPE uint64_t
#endif

//This defines the index of the comparison to be used when evaluating edges.  For 2 codes bits, there are 4 possibilities, 00, 01, 10, 11.
#if MAX_EDGE_WEIGHT <= POW2(8)
    #define EDGE_METRIC_INDEX_TYPE uint8_t 
#elif MAX_EDGE_WEIGHT <= POW2(16)
    #define EDGE_METRIC_INDEX_TYPE uint16_t 
#elif MAX_EDGE_WEIGHT <= POW2(32)
    #define EDGE_METRIC_INDEX_TYPE uint32_t 
#else
    #define EDGE_METRIC_INDEX_TYPE uint64_t 
#endif

#if TRACEBACK_LEN*k <= 8
    #define TRACEBACK_TYPE uint8_t
#elif TRACEBACK_LEN*k <= 16
    #define TRACEBACK_TYPE uint16_t
#elif TRACEBACK_LEN*k <= 32
    #define TRACEBACK_TYPE uint32_t
#else
    #define TRACEBACK_TYPE uint64_t
#endif

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

    //Change these to be pointers to fixed sized arrays
    //See https://stackoverflow.com/questions/1810083/c-pointers-pointing-to-an-array-of-fixed-size
    //Is under K&R C section 5.12 "Complicated Declarations"
    METRIC_TYPE (* restrict nodeMetricsCur)[NUM_STATES];
    TRACEBACK_TYPE (* restrict traceBackCur)[NUM_STATES];
    METRIC_TYPE (* restrict nodeMetricsNext)[NUM_STATES];
    TRACEBACK_TYPE (* restrict traceBackNext)[NUM_STATES];

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
int viterbiDecoderHard(viterbiHardState_t* restrict state, uint8_t* restrict codedSegments, uint8_t* restrict uncoded, int segmentsIn, bool last);

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

int argminPathMetrics(const METRIC_TYPE (*metrics)[POW2(k)]);

int argminNodeMetrics(const METRIC_TYPE (*metrics)[NUM_STATES]);

#endif