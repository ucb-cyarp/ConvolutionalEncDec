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

    for(int edgeInd = 0; edgeInd < POW2(k); edgeInd++){
        for(int stateInd = 0; stateInd < NUM_STATES; stateInd++){
            //Use the convolutional encoder functions to derive what coded segment corresponds to each edge
            resetConvEncoder(&tmpEncoder);
            tmpEncoder.tappedDelay = stateInd;
            //These edge metrics need to be re-ordered according to the shuffle network to be in the same order as the butterflies
            //Edit. actually, don't do thios because having the butterflies interleaved works better for vectorization
            // int newInd = ROTATE_RIGHT(stateInd, k, k*S);
            int newInd = stateInd;
            state->edgeCodedBits[edgeInd][newInd] = convEncOneInput(&tmpEncoder, edgeInd);
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
    int startingState = 0;
    // int newStartingIdx = ROTATE_RIGHT(startingState, k, k*S);
    int newStartingIdx = STARTING_STATE;

    state->nodeMetricsA[newStartingIdx] = 0;

    //Need to set the node metrics so that the initial path is the only non
    METRIC_TYPE forceNot = NUM_STATES+1;
    for(int i = 1; i<NUM_STATES; i++){
        // int newIdx = ROTATE_RIGHT(i, k, k*S);
        int newIdx = i;
        state->nodeMetricsA[newIdx] = forceNot;
    }

    // //Used to make sure we have the same starting point but is not strictly nessasary
    // for(int i = 0; i<TRACEBACK_BYTES; i++){
    //     for(int j=0; j<NUM_STATES; j++){
    //         state->tracebackBufs[i][j] = 0;
    //     }
    // }

    state->iteration = 0;
    state->decodeCarryOver = 0;
    state->decodeCarryOverCount = 0;
}

int viterbiDecoderHardButterflyk1(viterbiHardState_t* restrict state, uint8_t* restrict codedSegments, uint8_t* restrict uncoded, int segmentsIn, bool last){
    int segmentsOut = 0;

    for(unsigned int i = 0; i<segmentsIn; i++){
        uint8_t codedBits = codedSegments[i];
        // printf("Coded Segment: %2d, Seg: 0x%x\n", i, codedBits);

        METRIC_TYPE newMetrics[NUM_STATES];
        TRACEBACK_TYPE newTraceback[NUM_STATES];

        unsigned int tracebackByteIdx = (state->iteration*k)/8;

        uint8_t (* restrict tracebackBuf)[NUM_STATES] = &(state->tracebackBufs[tracebackByteIdx]);
        uint8_t tracebackBuf2[NUM_STATES];

        //Trellis Itteration
        for(unsigned int butterfly = 0; butterfly<(NUM_STATES/2); butterfly++){
            //Implement the 2 butterfly
            METRIC_TYPE a[2];
            a[0] = state->nodeMetricsA[butterfly] + calcHammingDist(state->edgeCodedBits[0][butterfly], codedBits, n);
            a[1] = state->nodeMetricsA[(NUM_STATES/2) + butterfly] + calcHammingDist(state->edgeCodedBits[0][(NUM_STATES/2) + butterfly], codedBits, n);

            METRIC_TYPE b[2];
            b[0] = state->nodeMetricsA[butterfly] + calcHammingDist(state->edgeCodedBits[1][butterfly], codedBits, n);
            b[1] = state->nodeMetricsA[(NUM_STATES/2) + butterfly] + calcHammingDist(state->edgeCodedBits[1][(NUM_STATES/2) + butterfly], codedBits, n);

            //It is essential to perform these operations without computing the index to select once
            //and then using that intermediate index to select both the metric and traceback
            //That extra level of indirection causes the compiler (at least clang) to not autovectorize this loop
            METRIC_TYPE aMetric = a[0] <= a[1] ? a[0] : a[1];
            METRIC_TYPE bMetric = b[0] <= b[1] ? b[0] : b[1];
            
            uint8_t aTraceback = a[0] <= a[1] ? 0 : 1;
            uint8_t bTraceback = b[0] <= b[1] ? 0 : 1;

            newMetrics[butterfly] = aMetric;
            newMetrics[(NUM_STATES/2) + butterfly] = bMetric;

            //Overwritting values seems to present a problem for the vectorizor.
            //For now, a temporary array is declared to hold the traceback
            //with the results copied back after this loop
            uint8_t oldTracebackA = (*tracebackBuf)[butterfly];
            uint8_t oldTracebackB = (*tracebackBuf)[(NUM_STATES/2) + butterfly];

            tracebackBuf2[butterfly] = (oldTracebackA << k) + aTraceback;
            tracebackBuf2[(NUM_STATES/2) + butterfly] =(oldTracebackB << k) + bTraceback;

            // printf("Min Path: %2d, Src Node: %2d Traceback: 0x%lx\n", minPathEdgeInIdx, minPathSrcNodeIdx, newTB);
        }

        for(unsigned int idx = 0; idx<NUM_STATES; idx++){
            (*tracebackBuf)[idx] = tracebackBuf2[idx];
        }

        //Perform the shuffle
        //Is an interleaving operation
        for(unsigned int idx = 0; idx<NUM_STATES/2; idx++){
            state->nodeMetricsA[idx*2] = newMetrics[idx];
            state->nodeMetricsA[idx*2+1] = newMetrics[NUM_STATES/2 + idx];
        }

        (state->iteration)++;

        //TODO: Implement block traceback
    }

    //Perform traceback
    //TODO: Support returning the reaminder of traceback after block traceback implemented
    if(last){
        //Shift the last round of tracebacks into their final positions (allows us to handle the last traceback index without more conditionals and should be trivially vectorizable)
        unsigned int lastTracebackByteIdx = ((state->iteration-1)*k)/8; //Byte containing last recorder bit idx.
        unsigned int numberBitsRecordedInLast = state->iteration*k - lastTracebackByteIdx*8; //Number of bits recordeded - bits recorded in previous bytes (lastTracebackByteIdx is indexed from 0 and also represents the number of full bytes before the last one)
        unsigned int remainingShift = 8-numberBitsRecordedInLast;
        for(int i = 0; i<NUM_STATES; i++){
            state->tracebackBufs[lastTracebackByteIdx][i] = state->tracebackBufs[lastTracebackByteIdx][i] << remainingShift;
        }

        //The number of traceback itterations is state->iteration-1
        unsigned int numPaddingSegments = S;

        //Select the terminated state
        uint8_t decodedLastState = 0;

        //Traceback padding segments
        for(unsigned int i = 0; i<numPaddingSegments; i++){
            unsigned int byteIdx = ((state->iteration-1-i)*k)/8;
            unsigned int segmentInByte = (((state->iteration-1-i)*k)%8)/k;
            unsigned int segmentInByteBitIdx = 7-segmentInByte*k;

            //Given a node index, we need to find the index this index is stored in before the reshuffeling (interleaving)
            //The node would be in a position before interleaving.  The group would be determined by the lower k LSbs
            //The position in the group would be determined by the remaining bits.  By right rotationally shifting the index
            //we get the stored position
            unsigned int storedTracebackNodeIdx = ROTATE_RIGHT(decodedLastState, 1, k*S);
            uint8_t decision = (state->tracebackBufs[byteIdx][storedTracebackNodeIdx] >> segmentInByteBitIdx) & (POW2(k)-1);

            //We do not store the decoded bits since they are padding.  If we did, it would be the k LSbs of the decoded state

            //Because the new bits are shifted left onto the LSb, we can get the origin node by shifting right then appending the decision as the MSbs.
            decodedLastState = (decodedLastState >> k) | (decision << ((S-1)*k));
        }

        //How many bytes are expected
        unsigned int lastDecodedByteIdx = (state->iteration-numPaddingSegments-1)*k/8;
        uncoded[lastDecodedByteIdx] = 0;

        //Zero out the last byte of the returned message since it may be partially filled
        //The other 

        for(unsigned int i = numPaddingSegments; i<state->iteration; i++){
            //Same routine as before except that we do now record the traceback
            unsigned int byteIdx = ((state->iteration-1-i)*k)/8;
            unsigned int segmentInByte = (((state->iteration-1-i)*k)%8)/k;
            unsigned int segmentInByteBitIdx = 7-segmentInByte*k;

            unsigned int storedTracebackNodeIdx = ROTATE_RIGHT(decodedLastState, 1, k*S);
            uint8_t decision = (state->tracebackBufs[byteIdx][storedTracebackNodeIdx] >> segmentInByteBitIdx) & (POW2(k)-1);

            //Get the decoded byte idx.  Because we are tracing back, we get the end of the message first
            //The last byte of the message may be partially filled
            unsigned int decodedSegmentIdx = state->iteration-1-i;
            unsigned int decodedByteIdx = decodedSegmentIdx*k/8;

            uint8_t decodedBits = decodedLastState & (POW2(k)-1);

            //The encoder transmits with the MSbs first then ends with the LSbs.  Since
            //we are tracing back, we start with the LSbs and end with the MSbs
            uncoded[decodedByteIdx] = (uncoded[decodedByteIdx] >> k) | (decodedBits << (8-k));

            //For that byte, we need to zero out the other 

            decodedLastState = (decodedLastState >> k) | (decision << ((S-1)*k));
        }

        segmentsOut = lastDecodedByteIdx+1;

        //Reset state for next packet
        resetViterbiDecoderHardButterflyk1(state);
    }

    return segmentsOut;
}