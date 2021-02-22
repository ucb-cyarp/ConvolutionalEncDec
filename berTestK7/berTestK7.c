#include "convEncode.h"
#include "viterbiDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define ENCODE_PKT_BYTE_LEN (1024)
#define PKTS (16)
#define ITERATIONS (10000)
#define PRINT_PERIOD (100)

#define CPU (16)

/**
 * Return a random number between 0 and 1 (inclusive)
 * 
 * @warning Resolution is limited by RAND_MAX
 */
double frand(){
    return (double) rand() / RAND_MAX;
}

/**
 * Corrupts coded array under the assumption that bit flips are IID
 */
int corruptCodedArray(uint8_t* orig, uint8_t* corrupted, int len, double errorProbability){
    //Recall, there are k bits per coded segment
    int corruptedBitCount = 0;
    for(int i = 0; i<len; i++){
        uint8_t bitCorrupt = 0;
        for(int j = 0; j<n; j++){
            uint8_t bitFlip = frand() > errorProbability ? 0 : 1;
            bitCorrupt = (bitCorrupt << 1) | bitFlip;
            corruptedBitCount += bitFlip;
        }
        corrupted[i] = orig[i]^bitCorrupt;
    }

    return corruptedBitCount;
}

int bitErrors(uint8_t* a, uint8_t* b, int len){
    int errorCount = 0;
    for(int i = 0; i<len; i++){
        int hammingDist = calcHammingDist(a[i], b[i]);
        errorCount += hammingDist;
    }

    return errorCount;
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

    printf("Rand Resolution: %e\n", (double) 1/RAND_MAX);

    srand(314);

    // double uncodedBer = 3.716174e-02;
    double uncodedBer = 2.262231e-02;

    //Initialize the Encoder
    convEncoderState_t convEncState;
    resetConvEncoder(&convEncState);
    initConvEncoder(&convEncState);

    //Initialize the Decoder
    viterbiHardState_t viterbiState;
    resetViterbiDecoderHard(&viterbiState);
    viterbiInit(&viterbiState);
    viterbiConfigCheck();

    int64_t codedBitsSent = 0;
    int64_t decodedBitsRecieved = 0;

    int64_t codedBitErrors = 0;
    int64_t decodedBitErrors = 0;

    for(int iter = 0; iter < ITERATIONS; iter++){
        if(iter % PRINT_PERIOD == 0){
            printf("Iteration %5d of %5d\n", iter, ITERATIONS);
        }

        //Create a random uncoded packet
        uint8_t uncodedPkt[ENCODE_PKT_BYTE_LEN];
        for(int j = 0; j<ENCODE_PKT_BYTE_LEN; j++){
            uncodedPkt[j] = (uint8_t) rand();
        }

        //Encode the packet
        uint8_t codedSegments[8*ENCODE_PKT_BYTE_LEN/k+S];
        int codedSegsReturned = convEnc(&convEncState, uncodedPkt, codedSegments, ENCODE_PKT_BYTE_LEN, true);
        //Can leave in for sanity check
        assert(codedSegsReturned == 8*ENCODE_PKT_BYTE_LEN/k+S);
        codedBitsSent += codedSegsReturned*n;

        //Corrupt the signal
        uint8_t corruptedCodedSegments[8*ENCODE_PKT_BYTE_LEN/k+S];
        int64_t codedBitFlips = corruptCodedArray(codedSegments, corruptedCodedSegments, 8*ENCODE_PKT_BYTE_LEN/k+S, uncodedBer);
        //Sanity check
        int64_t observedBitErrors = bitErrors(codedSegments, corruptedCodedSegments, 8*ENCODE_PKT_BYTE_LEN/k+S);
        assert(codedBitFlips == observedBitErrors);
        codedBitErrors += codedBitFlips;

        //Decode the signal
        uint8_t decodedBytes[ENCODE_PKT_BYTE_LEN];
        int decodedBytesReturned = viterbiDecoderHard(&viterbiState, corruptedCodedSegments, decodedBytes, 8*ENCODE_PKT_BYTE_LEN/k+S, true);
        //Can leave in for sanity check
        assert(decodedBytesReturned == ENCODE_PKT_BYTE_LEN);
        decodedBitsRecieved+=decodedBytesReturned*8;

        //Compare to origional signal
        int64_t uncodedBitErrorsPkt = bitErrors(uncodedPkt, decodedBytes, ENCODE_PKT_BYTE_LEN);
        decodedBitErrors += uncodedBitErrorsPkt;
    }

    printf("Desired Uncoded BER: %e, Actual Uncoded BER: %e [%ld/%ld], Coded BER: %e [%ld/%ld]\n", uncodedBer, (double) codedBitErrors/codedBitsSent, codedBitErrors, codedBitsSent, (double) decodedBitErrors/decodedBitsRecieved, decodedBitErrors, decodedBitsRecieved); //Note: The uncoded BER is the rate at which the transmitted bits were corrupted.  The bits sent were the coded bits.  The decoded BER is the BER after final decoding

    //TODO: Create version which feeds in half the data at a time to check the state carryover
    //TODO: Use better random number generator
    //TODO: Still a little different from the matlab prediction ... why?  Miscounting?  Seems to be reliably beter, not quite by half

    return 0;
}