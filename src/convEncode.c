#include "convEncode.h"
#include "exeParams.h"
#include <stdio.h>
#include <stdlib.h>

void resetConvEncoder(convEncoderState_t* state){
    state->tappedDelay = STARTING_STATE;

    state->remainingUncoded = 0;
    state->remainingUncodedCount = 0;
}

void initConvEncoder(convEncoderState_t* state){
    for(int i = 0; i<n; i++){
        state->polynomials[i] = bitReverseGenerator(g[i]);
    }
}

int convEncOneInput(convEncoderState_t* state, uint8_t bitsToShiftIn){
    //Shift in k-1 bits and shift the cursor each time
    //k >= 1
    int remainingBits = k;
    #if k!=1
        for(int j = 0; j<(k-1); j++){
            //Shift in the MSBs
            //TODO: Revisit after reciving new copy of Hacker's delight
            //      Need to bit reverse k bit segment before shifting in
            uint8_t bitToShiftIn = (bitsToShiftIn>>(remainingBits-1))%2;
            remainingBits--;
            state->tappedDelay = (state->tappedDelay << 1) | bitToShiftIn;
        }
    #endif
    //Perform the final shift in but not adjusting the cursor (which points to the next position to insert into)
    {
        //Shift in the MSBs
        uint8_t bitToShiftIn = bitsToShiftIn%2;
        //remainingBits--; Not needed
        state->tappedDelay = (state->tappedDelay << 1) | bitToShiftIn;
    }

    uint8_t codedSegment = computeEncOutputSegment(state);

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
                state->tappedDelay = (state->tappedDelay << 1) | bitToShiftIn;
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
            state->tappedDelay = (state->tappedDelay << 1) | bitToShiftIn;
        }

        codedSegments[segmentsOut] = computeEncOutputSegment(state);
        segmentsOut++;
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
                    state->tappedDelay = (state->tappedDelay << 1);
                }
            #endif

            state->tappedDelay = (state->tappedDelay << 1);

            codedSegments[segmentsOut] = computeEncOutputSegment(state);
            segmentsOut++;
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

uint8_t computeEncOutputSegment(convEncoderState_t* state){
        //Take the dot product mod 2 for each generator
        int codedBits[n];

        for(int genIdx = 0; genIdx<n; genIdx++){
            TAPPED_DELAY_TYPE masked = state->tappedDelay & state->polynomials[genIdx];
            
            //TODO try the xor version even if popcnt is present
            #if __has_builtin(__builtin_popcount)
                TAPPED_DELAY_TYPE sum = __builtin_popcount(masked);
                codedBits[genIdx] = sum%2;
            #else
                uint8_t parity = 0;
                for(int i = 0; i<k*K; i++){
                    parity ^= masked%2;
                    masked = masked >> 1;
                }
                codedBits[genIdx] = parity;
            #endif
        }

        //Output the 0th as the LSb
        //n>=1
        uint8_t codedSegment = (codedBits[n-1]%2);
        for(int j = n-2; j>=0; j--){
            codedSegment = codedSegment << 1;
            codedSegment |= (codedBits[j]%2);
        }
        return codedSegment;
}

TAPPED_DELAY_TYPE bitReverseGenerator(TAPPED_DELAY_TYPE packed){
    //TODO: Revisit after getting new copy of Hacker's Delight
    //Since this is only done once as the start of the program, speed is not critical

    TAPPED_DELAY_TYPE rtnVal = 0;
    for(int i = 0; i<k*K; i++){
        rtnVal = rtnVal << 1;
        rtnVal |= packed%2;
        packed = packed >> 1;
    }

    return rtnVal;
}