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

    //TODO: can actually support symmetry if only the input bit or last bit are relied on by all generators.
    //      It requires 2 comparisons per 
    #ifdef USE_POLY_SYMMETRY
        //We only record one entry per butterfly
        //Since there are 2 nodes per butterfly, there are half as many coded sequences computed for this table

        for(int i = 0; i < NUM_STATES/2; i++){
            int stateInd = i; //Computing the 0 edge from the first node in each butterfly.  The way the butterflies are interleaved, the first nodes in each butterfly come in sequence
            resetConvEncoder(&tmpEncoder);
            tmpEncoder.tappedDelay = stateInd;
            state->edgeCodedBitsSymm[i] = convEncOneInput(&tmpEncoder, 0);
        }
    #else
        for(int edgeInd = 0; edgeInd < POW2(k); edgeInd++){
            for(int stateInd = 0; stateInd < NUM_STATES; stateInd++){
                //Use the convolutional encoder functions to derive what coded segment corresponds to each edge
                resetConvEncoder(&tmpEncoder);
                tmpEncoder.tappedDelay = stateInd;
                //These edge metrics need to be re-ordered according to the shuffle network to be in the same order as the butterflies
                //Edit. actually, don't do thios because having the butterflies interleaved works better for vectorization
                int newInd = ROTATE_RIGHT(stateInd, k, k*S);
                // int newInd = stateInd;
                state->edgeCodedBits[edgeInd][newInd] = convEncOneInput(&tmpEncoder, edgeInd);
            }
        }
    #endif
}

void resetViterbiDecoderHardButterflyk1(viterbiHardState_t* state){
    state->nodeMetricsCur = &(state->nodeMetricsA);
    state->nodeMetricsNext = &(state->nodeMetricsB);

    state->traceBackCur = &(state->traceBackA);
    state->traceBackNext = &(state->traceBackB);

    //Note that the nodes are re-ordered going into the butterflies
    //No reordering needed if shuffle occures before buttertfly evaluation
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

        METRIC_TYPE newMetrics[NUM_STATES] __attribute__ ((aligned (32)));
        TRACEBACK_TYPE newTraceback[NUM_STATES] __attribute__ ((aligned (32)));

        unsigned int tracebackWordIdx = state->iteration;

        TRACEBACK_TYPE (* restrict tracebackBuf)[NUM_STATES] = &(state->tracebackBufs[tracebackWordIdx]);
        TRACEBACK_TYPE tracebackBuf2[NUM_STATES] __attribute__ ((aligned (32)));

        //Perform the shuffle
        //Is an interleaving operation

        //Trellis Itteration
        for(unsigned int butterfly = 0; butterfly<(NUM_STATES/2); butterfly++){
            //Implement the 2 butterfly
            #ifdef USE_POLY_SYMMETRY
                uint8_t edgeMetric = calcHammingDist(state->edgeCodedBitsSymm[butterfly], codedBits, n);
                // uint8_t xorValue = state->edgeCodedBitsSymm[butterfly] ^ codedBits;
                // uint8_t edgeMetric = (xorValue & 1) + ((xorValue >> 1)&1);
                uint8_t edgeMetricComplement = n-edgeMetric;

                METRIC_TYPE a[2];
                a[0] = state->nodeMetricsA[butterfly] + edgeMetric;
                a[1] = state->nodeMetricsA[NUM_STATES/2 + butterfly] + edgeMetricComplement;

                METRIC_TYPE b[2];
                b[0] = state->nodeMetricsA[butterfly] + edgeMetricComplement;
                b[1] = state->nodeMetricsA[NUM_STATES/2 + butterfly] + edgeMetric;
            #else
                METRIC_TYPE a[2];
                a[0] = state->nodeMetricsA[butterfly*2] + calcHammingDist(state->edgeCodedBits[0][butterfly*2], codedBits, n);
                a[1] = state->nodeMetricsA[butterfly*2+1] + calcHammingDist(state->edgeCodedBits[0][butterfly*2+1], codedBits, n);

                METRIC_TYPE b[2];
                b[0] = state->nodeMetricsA[butterfly*2] + calcHammingDist(state->edgeCodedBits[1][butterfly*2], codedBits, n);
                b[1] = state->nodeMetricsA[butterfly*2+1] + calcHammingDist(state->edgeCodedBits[1][butterfly*2+1], codedBits, n);
            #endif

            //It is essential to perform these operations without computing the index to select once
            //and then using that intermediate index to select both the metric and traceback
            //That extra level of indirection causes the compiler (at least clang) to not autovectorize this loop
            bool aDecision = a[0] > a[1];
            bool bDecision = b[0] > b[1];

            METRIC_TYPE aMetric = a[0];
            METRIC_TYPE bMetric = b[0];

            if(aDecision){
                aMetric = a[1];
            }
            if(bDecision){
                bMetric = b[1];
            }
            
            TRACEBACK_TYPE aTraceback = aDecision;
            TRACEBACK_TYPE bTraceback = bDecision;

            newMetrics[butterfly*2] = aMetric;
            newMetrics[butterfly*2+1] = bMetric;

            tracebackBuf2[butterfly*2] =  aTraceback;
            tracebackBuf2[butterfly*2+1] = bTraceback;

            // printf("Min Path: %2d, Src Node: %2d Traceback: 0x%lx\n", minPathEdgeInIdx, minPathSrcNodeIdx, newTB);
        }

        //Find the min path metric
        //The simple min approach did not vectorize well.  The compiler
        //inferred a bunch of branching
        //Instead, will do a tree reduction in stages
        
        if(state->iteration%64 == 0){
            // METRIC_TYPE minPathMetric = minMetricGeneric(&newMetrics);
            //The Compiler is not inlining the call for some reason
            //However, manually inlining it results in the compier
            //emitting a lot of conditionals around the outer loop ... 
            //which is strange.
            //It may be mucking up the inference the compiler makes
            //on the outer loop.  The additional conditionals result
            //in worse performance on my laptop/

            METRIC_TYPE minPathMetric = newMetrics[0];
            for(unsigned int idx = 1; idx<NUM_STATES; idx++){
                if(newMetrics[idx] < minPathMetric){
                    minPathMetric = newMetrics[idx];
                }
            }

            for(unsigned int idx = 0; idx<NUM_STATES; idx++){
                newMetrics[idx] = newMetrics[idx] - minPathMetric;
            }
        }

        for(unsigned int idx = 0; idx<NUM_STATES; idx++){
            (*tracebackBuf)[idx] = tracebackBuf2[idx];
        }

        for(unsigned int idx = 0; idx<NUM_STATES; idx++){
            state->nodeMetricsA[idx] = newMetrics[idx];
        }

        (state->iteration)++;

        //TODO: Implement block traceback
    }

    //Perform traceback
    //TODO: Support returning the reaminder of traceback after block traceback implemented
    if(last){
        //The number of traceback itterations is state->iteration-1
        unsigned int numPaddingSegments = S;

        //Select the terminated state
        uint8_t decodedLastState = 0;

        //Traceback padding segments
        for(unsigned int i = 0; i<numPaddingSegments; i++){
            unsigned int wordIdx = state->iteration-1-i;

            //Given a node index, we need to find the index this index is stored in before the reshuffeling (interleaving)
            //The node would be in a position before interleaving.  The group would be determined by the lower k LSbs
            //The position in the group would be determined by the remaining bits.  By right rotationally shifting the index
            //we get the stored position
            // unsigned int storedTracebackNodeIdx = ROTATE_RIGHT(decodedLastState, 1, k*S);
            unsigned int storedTracebackNodeIdx = decodedLastState;
            uint8_t decision = state->tracebackBufs[wordIdx][storedTracebackNodeIdx];

            //We do not store the decoded bits since they are padding.  If we did, it would be the k LSbs of the decoded state

            //Because the new bits are shifted left onto the LSb, we can get the origin node by shifting right then appending the decision as the MSbs.
            decodedLastState = (decodedLastState >> k) | (decision << ((S-1)*k));
        }

        //How many bytes are expected
        unsigned int lastDecodedWordIdx = (state->iteration-numPaddingSegments-1)*k/TRACEBACK_BITS;
        uncoded[lastDecodedWordIdx] = 0;

        //Zero out the last byte of the returned message since it may be partially filled
        //The other 

        for(unsigned int i = numPaddingSegments; i<state->iteration; i++){
            //Same routine as before except that we do now record the traceback
            unsigned int wordIdx = state->iteration-1-i;

            // unsigned int storedTracebackNodeIdx = ROTATE_RIGHT(decodedLastState, 1, k*S);
            unsigned int storedTracebackNodeIdx = decodedLastState;

            uint8_t decision = state->tracebackBufs[wordIdx][storedTracebackNodeIdx];

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

        segmentsOut = (state->iteration-numPaddingSegments-1)*k/8+1;

        //Reset state for next packet
        resetViterbiDecoderHardButterflyk1(state);
    }

    return segmentsOut;
}

//Note: Clang was able to infer the minimum operation in the general C implementation
//      Clang's implementation had fewer instructions than the explicit tree
//      reductions implemented below
// METRIC_TYPE minMetric2(const METRIC_TYPE (*metrics)[2]){
//     METRIC_TYPE minPathMetric = (*metrics)[0] < (*metrics)[1] ? (*metrics)[0] : (*metrics)[1];

//     return minPathMetric;
// }
// METRIC_TYPE minMetric4(const METRIC_TYPE (*metrics)[4]){
//     METRIC_TYPE minStage5[2];
//     for(unsigned int idx = 0; idx<2; idx++){
//         minStage5[idx] = (*metrics)[idx] < (*metrics)[2 + idx] ? (*metrics)[idx] : (*metrics)[2 + idx];
//     }

//     METRIC_TYPE minPathMetric = minStage5[0] < minStage5[1] ? minStage5[0] : minStage5[1];

//     return minPathMetric;
// }
// METRIC_TYPE minMetric8(const METRIC_TYPE (*metrics)[8]){
//     METRIC_TYPE minStage4[4];
//     for(unsigned int idx = 0; idx<4; idx++){
//         minStage4[idx] = (*metrics)[idx] < (*metrics)[4 + idx] ? (*metrics)[idx] : (*metrics)[4 + idx];
//     }

//     METRIC_TYPE minStage5[2];
//     for(unsigned int idx = 0; idx<2; idx++){
//         minStage5[idx] = minStage4[idx] < minStage4[2 + idx] ? minStage4[idx] : minStage4[2 + idx];
//     }

//     METRIC_TYPE minPathMetric = minStage5[0] < minStage5[1] ? minStage5[0] : minStage5[1];

//     return minPathMetric;
// }
// METRIC_TYPE minMetric16(const METRIC_TYPE (*metrics)[16]){
//     METRIC_TYPE minStage3[8];
//     for(unsigned int idx = 0; idx<8; idx++){
//         minStage3[idx] = (*metrics)[idx] < (*metrics)[8 + idx] ? (*metrics)[idx] : (*metrics)[8 + idx];
//     }

//     METRIC_TYPE minStage4[4];
//     for(unsigned int idx = 0; idx<4; idx++){
//         minStage4[idx] = minStage3[idx] < minStage3[4 + idx] ? minStage3[idx] : minStage3[4 + idx];
//     }

//     METRIC_TYPE minStage5[2];
//     for(unsigned int idx = 0; idx<2; idx++){
//         minStage5[idx] = minStage4[idx] < minStage4[2 + idx] ? minStage4[idx] : minStage4[2 + idx];
//     }

//     METRIC_TYPE minPathMetric = minStage5[0] < minStage5[1] ? minStage5[0] : minStage5[1];

//     return minPathMetric;
// }

// METRIC_TYPE minMetric32(const METRIC_TYPE (*metrics)[32]){
//     METRIC_TYPE minStage2[16];
//     for(unsigned int idx = 0; idx<16; idx++){
//         minStage2[idx] = (*metrics)[idx] < (*metrics)[16 + idx] ? (*metrics)[idx] : (*metrics)[16 + idx];
//     }

//     METRIC_TYPE minStage3[8];
//     for(unsigned int idx = 0; idx<8; idx++){
//         minStage3[idx] = minStage2[idx] < minStage2[8 + idx] ? minStage2[idx] : minStage2[8 + idx];
//     }

//     METRIC_TYPE minStage4[4];
//     for(unsigned int idx = 0; idx<4; idx++){
//         minStage4[idx] = minStage3[idx] < minStage3[4 + idx] ? minStage3[idx] : minStage3[4 + idx];
//     }

//     METRIC_TYPE minStage5[2];
//     for(unsigned int idx = 0; idx<2; idx++){
//         minStage5[idx] = minStage4[idx] < minStage4[2 + idx] ? minStage4[idx] : minStage4[2 + idx];
//     }

//     METRIC_TYPE minPathMetric = minStage5[0] < minStage5[1] ? minStage5[0] : minStage5[1];

//     return minPathMetric;
// }
// METRIC_TYPE minMetric64(const METRIC_TYPE (*metrics)[64]){
//     METRIC_TYPE minStage1[32];
//     for(unsigned int idx = 0; idx<32; idx++){
//         minStage1[idx] = (*metrics)[idx] < (*metrics)[32 + idx] ? (*metrics)[idx] : (*metrics)[32 + idx];
//     }

//     METRIC_TYPE minStage2[16];
//     for(unsigned int idx = 0; idx<16; idx++){
//         minStage2[idx] = minStage1[idx] < minStage1[16 + idx] ? minStage1[idx] : minStage1[16 + idx];
//     }

//     METRIC_TYPE minStage3[8];
//     for(unsigned int idx = 0; idx<8; idx++){
//         minStage3[idx] = minStage2[idx] < minStage2[8 + idx] ? minStage2[idx] : minStage2[8 + idx];
//     }

//     METRIC_TYPE minStage4[4];
//     for(unsigned int idx = 0; idx<4; idx++){
//         minStage4[idx] = minStage3[idx] < minStage3[4 + idx] ? minStage3[idx] : minStage3[4 + idx];
//     }

//     METRIC_TYPE minStage5[2];
//     for(unsigned int idx = 0; idx<2; idx++){
//         minStage5[idx] = minStage4[idx] < minStage4[2 + idx] ? minStage4[idx] : minStage4[2 + idx];
//     }

//     METRIC_TYPE minPathMetric = minStage5[0] < minStage5[1] ? minStage5[0] : minStage5[1];

//     return minPathMetric;
// }

inline METRIC_TYPE minMetricGeneric(const METRIC_TYPE (*metrics)[NUM_STATES]){
    //Note: the compiler was able to infer this as a min reduction
    //It looks like the key is to set the min to the first element
    //then itterate over the rest.  It makes it clear it is a min recuction.
    //Setting the min initially to be UINT8_MAX appears to leave the compiler
    //wondering if this is indeed a min reduction even though all metrics
    //must be less than UINT8_MAX.  Realizing that, however, would require
    //the compiler to realize UINT8_MAX is the max value the type can hold.
    METRIC_TYPE minPathMetric = (*metrics)[0];
    for(unsigned int idx = 1; idx<NUM_STATES; idx++){
        if((*metrics)[idx] < minPathMetric){
            minPathMetric = (*metrics)[idx];
        }
    }

    return minPathMetric;
}

// inline METRIC_TYPE minMetric(const METRIC_TYPE (*metrics)[NUM_STATES]){
//     // #if NUM_STATES==2
//     //     return minMetric2(metrics);
//     // #elif NUM_STATES==4
//     //     return minMetric4(metrics);
//     // #elif NUM_STATES==8
//     //     return minMetric8(metrics);
//     // #elif NUM_STATES==16
//     //     return minMetric16(metrics);
//     // #elif NUM_STATES==32
//     //     return minMetric32(metrics);
//     // #elif NUM_STATES==64
//     //     return minMetric64(metrics);
//     // #else
//     //     #warning Using generic min metric
//     //     return minMetricGeneric(metrics);
//     // #endif
    
//     //Clang properly inferred the min reduction
//     //for the generic description
//     //In fact, it contains fewer instructions
//     //than the tree reductions
//     return minMetricGeneric(metrics);
// }
