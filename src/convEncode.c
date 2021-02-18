#include "convEncode.h"
#include "exeParams.h"
#include <stdio.h>

inline void tappedDelayInsert(uint8_t* tappedDelay, int tappedDelayCursor, uint8_t input){
    tappedDelay[tappedDelayCursor] = input;
    tappedDelay[tappedDelayCursor+k*K] = input;
}

inline void tappedDelayStep(int* tappedDelayCursor){
    int oldInd = *tappedDelayCursor;
    if(oldInd == 0){
        *tappedDelayCursor = k*K-1;
    }else{
        *tappedDelayCursor = oldInd-1;
    }
}

void setConvEncoderState(convEncoderState_t* state, int tappedDelayStateRequested){
    for(int i = 0; i<k*K; i++){
        tappedDelayInsert(state->tappedDelay, i, tappedDelayStateRequested%2);
        tappedDelayStateRequested = tappedDelayStateRequested >> 1;
    }
    state->tappedDelayCursor = 0; //This was set to the LSb of tappedDelayStateRequested
    tappedDelayStep(&(state->tappedDelayCursor));
}

void resetConvEncoder(convEncoderState_t* state){
    setConvEncoderState(state, STARTING_STATE);

    state->remainingUncoded = 0;
    state->remainingUncodedCount = 0;
}

void initConvEncoder(convEncoderState_t* state){
    for(int i = 0; i<n; i++){
        unpackBigToLittleEndian(state->polynomials[i], k*K, g[i]);
    }
}

int convEncOneInput(convEncoderState_t* state, uint8_t bitsToShiftIn){
    //Shift in k-1 bits and shift the cursor each time
    //k >= 1
    int remainingBits = k;
    #if k!=1
        for(int j = 0; j<(k-1); j++){
            //Shift in the MSBs
            uint8_t bitToShiftIn = (bitsToShiftIn>>(remainingBits-1))%2;
            remainingBits--;
            tappedDelayInsert(state->tappedDelay, state->tappedDelayCursor, bitToShiftIn);
            tappedDelayStep(&(state->tappedDelayCursor));
        }
    #endif
    //Perform the final shift in but not adjusting the cursor (which points to the next position to insert into)
    {
        //Shift in the MSBs
        uint8_t bitToShiftIn = bitsToShiftIn%2;
        //remainingBits--; Not needed
        tappedDelayInsert(state->tappedDelay, state->tappedDelayCursor, bitToShiftIn);
    }

    uint8_t codedSegment = computeEncOutputSegment(state);

    //Perform the final shift of the tapped delay cursor
    tappedDelayStep(&(state->tappedDelayCursor));

    return codedSegment;
}

int convEnc(convEncoderState_t* state, uint8_t* uncoded, uint8_t* codedSegments, int bytesIn, bool last){

    int remainingBits = state->remainingUncodedCount;
    int newBits = bytesIn*8;

    int bitsToEncode = remainingBits + newBits;
    int chunksToEncode = bitsToEncode/k;

    uint8_t workingBits = state->remainingUncoded;
    int nextByteInd = 0;
    int segmentsOut = 0;

    //Encode the chunks
    for(int i = 0; i<chunksToEncode; i++){
        
        //Shift in k-1 bits and shift the cursor each time
        //k >= 1
        #if k!=1
            for(int j = 0; j<(k-1); j++){
                if(remainingBits == 0){
                    //refill working bits
                    workingBits = uncoded[nextByteInd];
                    nextByteInd++;
                    remainingBits = 8;
                }

                //Shift in the MSBs
                uint8_t bitToShiftIn = (workingBits>>(remainingBits-1))%2;
                remainingBits--;
                tappedDelayInsert(state->tappedDelay, state->tappedDelayCursor, bitToShiftIn);
                tappedDelayStep(&(state->tappedDelayCursor));
            }
        #endif
        //Perform the final shift in but not adjusting the cursor (which points to the next position to insert into)
        {
            if(remainingBits == 0){
                //refill working bits
                workingBits = uncoded[nextByteInd];
                nextByteInd++;
                remainingBits = 8;
            }

            //Shift in the MSBs
            uint8_t bitToShiftIn = (workingBits>>(remainingBits-1))%2;
            remainingBits--;
            tappedDelayInsert(state->tappedDelay, state->tappedDelayCursor, bitToShiftIn);
        }

        codedSegments[segmentsOut] = computeEncOutputSegment(state);
        segmentsOut++;

        //Perform the final shift of the tapped delay cursor
        tappedDelayStep(&(state->tappedDelayCursor));
    }

    //Padding
    if(last){
        //TODO: Remove check
        if(remainingBits>0){
            printf("Recieved last flag while bits remaining to be coded are present.  Make sure the number of bits in the message is both a multiple of 8 and k\n");
            exit(1);
        }

        for(int i = 0; i<S; i++){
            #if k!=1
                for(int j = 0; j<(k-1); j++){
                    tappedDelayInsert(state->tappedDelay, state->tappedDelayCursor, 0);
                    tappedDelayStep(&(state->tappedDelayCursor));
                }
            #endif

            tappedDelayInsert(state->tappedDelay, state->tappedDelayCursor, 0);

            codedSegments[segmentsOut] = computeEncOutputSegment(state);
            segmentsOut++;

            //Perform the final shift of the tapped delay cursor
            tappedDelayStep(&(state->tappedDelayCursor));
        }

        //Reset state for next packet
        resetConvEncoder(state);
    }else{
        //Store remaining bits and ind
        state->remainingUncodedCount = remainingBits;
        state->remainingUncoded = workingBits;
    }

    return segmentsOut;
}

inline uint8_t computeEncOutputSegment(convEncoderState_t* state){
        //Take the dot product mod 2 for each generator, using the current tapped delay cursor.
        //Circular buffer is implemented so that this is always the full buffer
        int codedBits[n];

        //TODO: Try swapping the order of these 2 for loops
        for(int genIdx = 0; genIdx<n; genIdx++){
            for(int j = 0; j<k*K; j++){ //Need the full vector here
                codedBits[genIdx] += state->tappedDelay[state->tappedDelayCursor+j]*state->polynomials[genIdx][j];
            }
        }

        //Output the 0th as the LSb
        //n>=1
        uint8_t codedSegment = (codedBits[n-1]%2);
        for(int j = n-2; j>=0; j--){
            codedSegment = codedSegment << 1;
            codedSegment |= (codedBits[j]%2);
        }
}