#include "viterbiDecoderButterflyk1.h"
#include "convEncode.h"
#include <stdio.h>
#include "exeParams.h"
#include <stdbool.h>
#include <stdlib.h>

void viterbiInitButterflyk1(viterbiHardState_t* state){
    //Populate the edgeCompareIdx entries.

    convEncoderState_t tmpEncoder;
    resetConvEncoder(&tmpEncoder);
    initConvEncoder(&tmpEncoder);

    // printf("Decoder Trellis Init\n");
    printf("Specialized Viterbi Decoder for k=1\n");

    for(int stateInd = 0; stateInd < NUM_STATES; stateInd++){
        for(int edgeInd = 0; edgeInd < POW2(k); edgeInd++){
            //Use the convolutional encoder functions to derive what coded segment corresponds to each edge
            resetConvEncoder(&tmpEncoder);
            tmpEncoder.tappedDelay = stateInd;
            //These edge metrics need to be re-ordered according to the shuffle network to be in the same order as the butterflies
            //Edit. actually, don't do thios because having the butterflies interleaved works better for vectorization
            // int newInd = ROTATE_RIGHT(stateInd, k, k*S);
            int newInd = stateInd;
            state->edgeCodedBits[newInd][edgeInd] = convEncOneInput(&tmpEncoder, edgeInd);
        }
    }
}

void resetViterbiDecoderHardButterflyk1(viterbiHardState_t* state){
    state->nodeMetricsCur = &(state->nodeMetricsA);
    state->nodeMetricsNext = &(state->nodeMetricsB);

    state->traceBackCur = &(state->traceBackA);
    state->traceBackNext = &(state->traceBackB);

    //Note that the nodes are re-ordered going into the butterflies
    //Edit. actually, don't do thios because having the butterflies interleaved works better for vectorization
    int startingState = STARTING_STATE;
    // int newStartingIdx = ROTATE_RIGHT(startingState, k, k*S);
    int newStartingIdx = STARTING_STATE;

    (*state->nodeMetricsCur)[newStartingIdx] = 0;

    //Need to set the node metrics so that the initial path is the only non
    METRIC_TYPE forceNot = NUM_STATES+1;
    for(int i = 0; i<NUM_STATES; i++){
        if(i != STARTING_STATE){
            // int newIdx = ROTATE_RIGHT(i, k, k*S);
            int newIdx = i;
            (*state->nodeMetricsCur)[newIdx] = forceNot;
        }

        (*state->traceBackCur)[i] = 0;
    }

    state->iteration = 0;
    state->decodeCarryOver = 0;
    state->decodeCarryOverCount = 0;
}

int viterbiDecoderHardButterflyk1(viterbiHardState_t* restrict state, uint8_t* restrict codedSegments, uint8_t* restrict uncoded, int segmentsIn, bool last){
    int segmentsOut = 0;

    for(int i = 0; i<segmentsIn; i++){
        uint8_t codedBits = codedSegments[i];
        // printf("Coded Segment: %2d, Seg: 0x%x\n", i, codedBits);

        //Compute the edge metrics
        //Compute the hamming distance for each possible codeword segment
        uint8_t edgeMetrics[POW2(n)];
        for(int j = 0; j<POW2(n); j++){
            edgeMetrics[j] = calcHammingDist(j, codedBits, n);
            // printf("Edge Metric [%d]: %d\n", j, edgeMetrics[j]);
        }

        //Trellis Itteration
        for(int butterfly = 0; butterfly<(NUM_STATES/2); butterfly++){
            //Implement the 2 butterfly
            METRIC_TYPE a[2];
            a[0] = (*state->nodeMetricsCur)[butterfly] + edgeMetrics[state->edgeCodedBits[butterfly][0]];
            a[1] = (*state->nodeMetricsCur)[(NUM_STATES/2) + butterfly] + edgeMetrics[state->edgeCodedBits[(NUM_STATES/2) + butterfly][0]];

            METRIC_TYPE b[2];
            b[0] = (*state->nodeMetricsCur)[butterfly] + edgeMetrics[state->edgeCodedBits[butterfly][1]];
            b[1] = (*state->nodeMetricsCur)[(NUM_STATES/2) + butterfly] + edgeMetrics[state->edgeCodedBits[(NUM_STATES/2) + butterfly][1]];

            int a_select = a[0] <= a[1] ? 0 : 1;
            int b_select = b[0] <= b[1] ? 0 : 1;

            (*state->nodeMetricsCur)[butterfly] = a[a_select];
            (*state->nodeMetricsCur)[(NUM_STATES/2) + butterfly] = b[b_select];

            TRACEBACK_TYPE traceback[2];
            traceback[0] = (*state->traceBackCur)[butterfly];
            traceback[1] = (*state->traceBackCur)[(NUM_STATES/2) + butterfly];

            (*state->traceBackCur)[butterfly] = traceback[a_select] << 1; //Shifts on zero
            (*state->traceBackCur)[(NUM_STATES/2) + butterfly] = (traceback[b_select] << 1) + 1; //Shifts on 1

            // printf("Min Path: %2d, Src Node: %2d Traceback: 0x%lx\n", minPathEdgeInIdx, minPathSrcNodeIdx, newTB);
        }

        //Perform the shuffle
        //Is an interleaving operation
        for(int idx = 0; idx<NUM_STATES/2; idx++){
            (*state->nodeMetricsNext)[idx*2] = (*state->nodeMetricsCur)[idx];
            (*state->nodeMetricsNext)[idx*2+1] = (*state->nodeMetricsCur)[NUM_STATES/2 + idx];
        }

        for(int idx = 0; idx<NUM_STATES/2; idx++){
            (*state->traceBackNext)[idx*2] = (*state->traceBackCur)[idx];
            (*state->traceBackNext)[idx*2+1] = (*state->traceBackCur)[NUM_STATES/2 + idx];
        }

        (state->iteration)++;

        swapViterbiArrays(state);

        // for(int dstState = 0; dstState<NUM_STATES; dstState++){
        //     printf("State: %2d, Metric: %2d, Traceback: 0x%lx\n", dstState, state->nodeMetricsCur[dstState], state->traceBackCur[dstState]);
        // }

        //Implement Traceback + Check if Traceback is Ready
        if(state->iteration >= TRACEBACK_LEN){
            //Make decision on traceback
            //Find current minimum node:
            int minNodeIdx = argminNodeMetrics(state->nodeMetricsCur);
            // int minNodeIdx = argminNodeMetrics(state->nodeMetricsCur);
            TRACEBACK_TYPE nodeTB = (*state->traceBackCur)[minNodeIdx];

            //Fetch the results the traceback length back
            //For example, lets say the Traceback length is 1 and k=2.  The traceback buffer does not need to be shifted
            TRACEBACK_TYPE tracebackSeg = (nodeTB >> ((TRACEBACK_LEN-1)*k)) % POW2(k);

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

        TRACEBACK_TYPE tb = (*state->traceBackCur)[0];

        // printf("Traceback: 0x%lx\n", tb>>(S*k));

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
        resetViterbiDecoderHardButterflyk1(state);
    }

    return segmentsOut;
}