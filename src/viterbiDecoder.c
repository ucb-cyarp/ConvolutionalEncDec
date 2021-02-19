#include "viterbiDecoder.h"
#include "convEncode.h"
#include <stdio.h>
#include "exeParams.h"
#include <stdbool.h>
#include <stdlib.h>


int viterbiConfigCheck(){
    if(sizeof(TRACEBACK_TYPE)*8 < TRACEBACK_LEN){
        printf("Traceback type cannot accomodate requested traceback length\n");
        exit(1);
    }

    return 0;
}

void viterbiInit(viterbiHardState_t* state){
    //Populate the edgeCompareIdx entries.

    convEncoderState_t tmpEncoder;
    resetConvEncoder(&tmpEncoder);
    initConvEncoder(&tmpEncoder);

    // printf("Decoder Trellis Init\n");

    for(int stateInd = 0; stateInd < NUM_STATES; stateInd++){
        for(int edgeInd = 0; edgeInd < POW2(k); edgeInd++){
            //Use the convolutional encoder functions to derive what coded segment corresponds to each edge
            resetConvEncoder(&tmpEncoder);
            setConvEncoderState(&tmpEncoder, stateInd);
            state->edgeCodedBits[stateInd][edgeInd] = convEncOneInput(&tmpEncoder, edgeInd);
            // printf("State: %2d, Edge: %2d, Coded Bits: 0x%x\n", stateInd, edgeInd, state->edgeCodedBits[stateInd][edgeInd]);
        }
    }
}

int viterbiDecoderHard(viterbiHardState_t* state, uint8_t* codedSegments, uint8_t* uncoded, int segmentsIn, bool last){
    //If the convolutional encoder forces the end of the message to be in the zero state, it allows us to
    //pad the last block with all zero codewords which, since the generating polynomials do not include nots,
    //would simply perpetuate the all zero path.  The metric of this path would not change.
    //The other node metric would either not change (when 00 is an outward edge of a path) or would become larger.
    //Unless there is a cycle with 00 output in the encoder FSM, after enough padding, the all zero path would be forced
    //to be the minimum.
    //
    //However, since I am performing the minimum node comparison to limit the traceback length, forcing to the 
    //all zero state and simply taking that path is not the objective.  I still need to take the minimum in every 
    //cycle.  However, my hope is that the minimum path would not change by doing this.  If we assume the minimum path
    //at the end of the message was the all zero state (as it should have been), then this would be true.  If, however,
    //enough errors were encountered such that the all zero state is not the minimum, the minimum path would change with
    //the zero padding.  Arguably, this would be a correction since we have a priori information that the last state is
    //the all zero state.  In theory, it could take many padded zero itterations before the minimum path settled to the
    //
    //Since we are limiting traceback, we are already living with a suboptimal implementation of the algorithm.
    //What I propose is running enough 0 codewords at the ends to pad out the block.  Then the remaining traceback
    //is taken from node 0.  This may not be the minimum path but we know it is the correct final state.
    //It may result in a discontinuity if the path difference extended back to the last traced back bit 
    //(which will be the traceback len - padding length).  However, if this occured, we will consider there to have
    //been a catastrophic failure of the FEC coding and that other recovery steps, such as retransmission, need
    //to be used

    //TODO: Create version which uses a fixed block size and a version which just uses the number of input segments

    int segmentsOut = 0;

    for(int i = 0; i<segmentsIn; i++){
        uint8_t codedBits = codedSegments[i];
        // printf("Coded Segment: %2d, Seg: 0x%x\n", i, codedBits);

        //Compute the edge metrics
        //Compute the hamming distance for each possible codeword segment
        uint8_t edgeMetrics[POW2(n)];
        for(int j = 0; j<POW2(n); j++){
            edgeMetrics[j] = calcHammingDist(j, codedBits);
            // printf("Edge Metric [%d]: %d\n", j, edgeMetrics[j]);
        }

        //TODO: Implement Trellis Itteration
        METRIC_TYPE* newMetrics = state->nodeMetricsNext;
        TRACEBACK_TYPE* newTraceback = state->traceBackNext;
        for(int dstState = 0; dstState<NUM_STATES; dstState++){
            //Since we are itterating on the destinations, we will be computing
            //the path metrics for each incoming edge

            int edgeOut = dstState % POW2(k);

            METRIC_TYPE pathMetrics[POW2(k)];
            for(int edgeIn = 0; edgeIn<POW2(k); edgeIn++){
                //The nodes to select from are DstNodeIdx/(2^k) + i*2^((S-1)*k)
                int srcNodeIdx = dstState/POW2(k) + edgeIn*POW2((S-1)*k);
                METRIC_TYPE srcMetric = state->nodeMetricsCur[srcNodeIdx];

                int edgeMetricIdx = state->edgeCodedBits[srcNodeIdx][edgeOut];

                pathMetrics[edgeIn] = srcMetric + edgeMetrics[edgeMetricIdx];

                // printf("Path Metric [%2d][%2d] = %d\n", dstState, edgeIn, pathMetrics[edgeIn]);
            }

            //Find the minimum weight path metric
            int minPathEdgeInIdx = argminPathMetrics(pathMetrics);
            int minPathSrcNodeIdx = dstState/POW2(k) + minPathEdgeInIdx*POW2((S-1)*k);
            newMetrics[dstState] = pathMetrics[minPathEdgeInIdx];

            //Copy the traceback from the minimum path and shift left by k
            //Append the bits corresponding to the edges coming into this node
            //   - They are all the same and are the k LSbs of the node index
            TRACEBACK_TYPE newTB = state->traceBackCur[minPathSrcNodeIdx];
            newTB = newTB << k;
            newTB |= edgeOut;
            newTraceback[dstState] = newTB;

            // printf("Min Path: %2d, Src Node: %2d Traceback: 0x%lx\n", minPathEdgeInIdx, minPathSrcNodeIdx, newTB);
        }

        //Update the state
        swapViterbiArrays(state);

        (state->iteration)++;

        for(int dstState = 0; dstState<NUM_STATES; dstState++){
            printf("State: %2d, Metric: %2d, Traceback: 0x%lx\n", dstState, state->nodeMetricsCur[dstState], state->traceBackCur[dstState]);
        }

        //Implement Traceback + Check if Traceback is Ready
        if(state->iteration >= TRACEBACK_LEN){
            //Make decision on traceback
            //Find current minimum node:
            int minNodeIdx = argminNodeMetrics(state->nodeMetricsCur);
            TRACEBACK_TYPE nodeTB = state->traceBackCur[minNodeIdx];

            //Fetch the results the traceback length back
            //For example, lets say the Traceback length is 1 and k=2.  The traceback buffer does not need to be shifted
            TRACEBACK_TYPE tracebackSeg = (nodeTB >> ((TRACEBACK_LEN-1)*k)) % k;

            //Pack the traceback

            #if (8%k) == 0
                //The packing operation is easier if k divides 8
                state->decodeCarryOver = (state->decodeCarryOver << k) | tracebackSeg;
                state->decodeCarryOverCount += k;

                if(state->decodeCarryOverCount == 8){
                    uncoded[segmentsOut] = state->decodeCarryOver;
                    state->decodeCarryOverCount = 0;
                    segmentsOut++;
                }
            #else
                //Need to handle the case when there may be spillover
                if(state->decodeCarryOverCount + k > 8){
                    //Spillover has occured
                    //Need to split the traceback
                    //Shift in the MSbs
                    int amountToShiftIn = 8 - state->decodeCarryOverCount;
                    int remainingToShiftIn = k-amountToShiftIn;

                    state->decodeCarryOver = (state->decodeCarryOver << amountToShiftIn) | ((tracebackSeg >> remainingToShiftIn)%POW2(amountToShiftIn));
                    uncoded[segmentsOut] = state->decodeCarryOver;
                    // state->decodeCarryOverCount = 0;

                    state->decodeCarryOver = (state->decodeCarryOver << remainingToShiftIn) | (remainingToShiftIn%POW2(remainingToShiftIn));
                    state->decodeCarryOverCount = remainingToShiftIn;

                }else{
                    //Can just shift in the traceback
                    state->decodeCarryOver = (state->decodeCarryOver << k) | tracebackSeg;
                    state->decodeCarryOverCount += k;

                    if(state->decodeCarryOverCount == 8){
                        uncoded[segmentsOut] = state->decodeCarryOver;
                        state->decodeCarryOverCount = 0;
                        segmentsOut++;
                    }
                }
            #endif
        }
    }

    //Handle Checking for Final Traceback and reset
    if(last){
        //Get the remaining K-1 segments of traceback
        //Note, there may not be a full K-1 segments of traceback if the message is short
        int remainingTraceback = (state->iteration < (TRACEBACK_LEN-1) ? state->iteration : TRACEBACK_LEN-1);

        //Since the state of the encoder was forced back to 0, we can just take the traceback from node 0
        //TODO: Change if padding is later removed

        TRACEBACK_TYPE tb = state->traceBackCur[0];

        printf("Traceback: 0x%lx\n", tb>>(S*k));

        //Exclude the final padding
        //The extra padding is S segments long
        //Shift out the last S segments of traceback
        tb = tb >> (S*k);
        remainingTraceback -= S;

        for(int i = remainingTraceback-1; i>=0; i--){
            state->decodeCarryOver = (state->decodeCarryOver << k) | ((tb >> (i*k))%POW2(k));
            state->decodeCarryOverCount += k;

            if(state->decodeCarryOverCount == 8){
                uncoded[segmentsOut] = state->decodeCarryOver;
                state->decodeCarryOverCount = 0;
                segmentsOut++;
            }
        }

        //TODO: Remove check
        if(state->decodeCarryOverCount != 0){
            printf("After removing padding, decoded message should be in multiples of 8 bits\n");
            exit(1);
        }

        //Reset state for next packet
        resetViterbiDecoderHard(state);
    }

    return segmentsOut;
}

void resetViterbiDecoderHard(viterbiHardState_t* state){
    state->nodeMetricsCur = state->nodeMetricsA;
    state->nodeMetricsNext = state->nodeMetricsB;

    state->traceBackCur = state->traceBackA;
    state->traceBackNext = state->traceBackB;

    state->nodeMetricsCur[STARTING_STATE] = 0;

    //Need to set the node metrics so that the initial path is the only non
    METRIC_TYPE forceNot = NUM_STATES+1;
    for(int i = 0; i<NUM_STATES; i++){
        if(i != STARTING_STATE){
            state->nodeMetricsCur[i] = forceNot;
        }

        state->traceBackCur[i] = 0;
    }

    state->iteration = 0;
    state->decodeCarryOver = 0;
    state->decodeCarryOverCount = 0;
}

uint8_t calcHammingDist(uint8_t a, uint8_t b){
    uint8_t bitDifferences = a^b;

    //Need to count the number of ones in the bit differences
    //x86 has an instruction to do this called POPCNT (mentioned in 
    //https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer 
    //and documented in https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=popcnt)

    //Note that __has_builtin was added to GCC and has been in clang for a while
    //https://stackoverflow.com/questions/54079257/how-to-check-builtin-function-is-available-on-gcc
    //there is a popcount buitin: __builtin_popcount (https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html)

    uint8_t distance;
    #if __has_builtin(__builtin_popcount)
        distance = (uint8_t) __builtin_popcount(bitDifferences);
    #else
        distance = 0;
        for(int i = 0; i<8; i++){
            distance += bitDifferences%2;
            bitDifferences = bitDifferences >> 1;
        }
    #endif
    
    return distance;
}

int argminPathMetrics(METRIC_TYPE *metrics){
    //There are 2^k paths to check
    //Do this in a tree fashion - hopefully it gives the compiler opertunities to overlap computatation

    //TODO: Check performance

    //Since the number of metrics is a power of 2, the number of tree steps is k
    METRIC_TYPE workingInd[POW2(k)/2];

    //Do the first pairwise 
    for(int i = 0; i<POW2(k)/2; i++){
        int indA = i*2;
        int indB = i*2+1;

        if(metrics[indA] <= metrics[indB]){
            workingInd[i] = indA;
        }else{
            workingInd[i] = indB;
        }
    }

    //Do the remaining 
    for(int i = k-1; i>0; i--){
        int numComparisons = POW2(i-1);
        for(int j = 0; j<numComparisons; j++){
            int indA = workingInd[j*2];
            int indB = workingInd[j*2+1];

            if(metrics[indA] <= metrics[indB]){
                workingInd[j] = indA;
            }else{
                workingInd[j] = indB;
            }
        }
    }

    return workingInd[0];
}

int argminNodeMetrics(METRIC_TYPE *metrics){
    //There are 2^k paths to check
    //Do this in a tree fashion - hopefully it gives the compiler opertunities to overlap computatation

    //TODO: Check performance

    //Since the number of metrics is a power of 2, the number of tree steps is k
    METRIC_TYPE workingInd[POW2(k*S)/2];

    //Do the first pairwise 
    for(int i = 0; i<POW2(k*S)/2; i++){
        int indA = i*2;
        int indB = i*2+1;

        if(metrics[indA] <= metrics[indB]){
            workingInd[i] = indA;
        }else{
            workingInd[i] = indB;
        }
    }

    //Do the remaining 
    for(int i = k*S-1; i>0; i--){
        int numComparisons = POW2(i-1);
        for(int j = 0; j<numComparisons; j++){
            int indA = workingInd[j*2];
            int indB = workingInd[j*2+1];

            if(metrics[indA] <= metrics[indB]){
                workingInd[j] = indA;
            }else{
                workingInd[j] = indB;
            }
        }
    }

    return workingInd[0];
}

void swapViterbiArrays(viterbiHardState_t* state){
    METRIC_TYPE* tmpMetric = state->nodeMetricsCur;
    state->nodeMetricsCur = state->nodeMetricsNext;
    state->nodeMetricsNext = tmpMetric;

    TRACEBACK_TYPE* tmpTraceback = state->traceBackCur;
    state->traceBackCur = state->traceBackNext;
    state->traceBackNext = tmpTraceback;
}