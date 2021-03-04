#include "convEncode.h"
#include "viterbiDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#define ENCODE_PKT_BYTE_LEN (10240)
#define PKTS (1000)
#define PRINT_PERIOD (100)
#define RAND_SEED (9865)
//#define PRINT_PROGRESS

#define REL_ERROR_THRESH (0.1)

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
        int hammingDist = calcHammingDist(a[i], b[i], 8);
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

    srand(RAND_SEED);

    printf("Rand Resolution: %e\n", (double) 1/RAND_MAX);
    printf("Rand Seed: %d\n", RAND_SEED);

    printf("\n");
    printf("Pkts: %d\n", PKTS);
    printf("Bytes/Pkt: %d\n", ENCODE_PKT_BYTE_LEN);
    printf("\n");
    printf("Test Error Threshold: %%%6.2f\n", REL_ERROR_THRESH*100);

    printf("\n");

    //NOTE: These numbers were obtained from Matlab using the 
    //vitdec functions parameterized with the same settings as
    //this decoder.  Note that the simulation did not 
    //
    //Previous attempts using the poly2trellis, distspec, and bercoding 
    //functions presented problems.
    //distspec takes 2 arguments, the trellis and the number
    //of components of the weight and distance spectra to find (n).
    //The second parameter is optional.  Note that different
    //values of n can result in substantially different expected BER
    //rates.  For example, for the rate 1/2 code with constraint length
    //7 and generators 0133 and 0171 (octal), the expected coded BER 
    //corresponding to the uncoded BERs of [3.716174e-02, 2.262231e-02, 
    //1.232962e-02] is [2.835189e-04, 2.490713e-05, 1.240189e-06].
    //However, with n=10, the expected coded BER becomes
    //[1.104553e-03, 5.016878e-05, 1.711085e-06].
    double snr[] = {-5, -4, -3};
    double uncodedBer[] = {5.585640e-02, 3.716174e-02, 2.262231e-02};
    double expectedCodedBer[] = {5.295410e-03, 5.421997e-04, 3.385010e-05};
    int numConfigs = sizeof(snr)/sizeof(snr[0]);

    printf("** SNR is for 4 Samples Per Symbol **\n");
    printf("** Expected Values from Matlab Simulations **\n");
    printf("   SNR | Expected Uncoded BER  Achieved Uncoded BER  Bit Errors    Bits Sent | Expected Coded BER  Measured Coded BER  Bit Errors    Bits Sent    Error\n");

    bool failed = false;

    for(int configInd = 0; configInd<numConfigs; configInd++){
        //Initialize the Encoder
        convEncoderState_t convEncState;
        resetConvEncoder(&convEncState);
        initConvEncoder(&convEncState);

        //Initialize the Decoder
        viterbiHardState_t viterbiState;
        VITERBI_RESET(&viterbiState);
        VITERBI_INIT(&viterbiState);
        viterbiConfigCheck();

        int64_t codedBitsSent = 0;
        int64_t decodedBitsRecieved = 0;

        int64_t codedBitErrors = 0;
        int64_t decodedBitErrors = 0;

        for(int iter = 0; iter < PKTS; iter++){
            #ifdef PRINT_PROGRESS
                if(iter % PRINT_PERIOD == 0){
                    printf("Iteration %5d of %5d\n", iter, PKTS);
                }
            #endif

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
            int64_t codedBitFlips = corruptCodedArray(codedSegments, corruptedCodedSegments, 8*ENCODE_PKT_BYTE_LEN/k+S, uncodedBer[configInd]);
            //Sanity check
            int64_t observedBitErrors = bitErrors(codedSegments, corruptedCodedSegments, 8*ENCODE_PKT_BYTE_LEN/k+S);
            assert(codedBitFlips == observedBitErrors);
            codedBitErrors += codedBitFlips;

            //Decode the signal
            uint8_t decodedBytes[ENCODE_PKT_BYTE_LEN];
            int decodedBytesReturned = VITERBI_DECODER_HARD(&viterbiState, corruptedCodedSegments, decodedBytes, 8*ENCODE_PKT_BYTE_LEN/k+S, true);
            //Can leave in for sanity check
            assert(decodedBytesReturned == ENCODE_PKT_BYTE_LEN);
            decodedBitsRecieved+=decodedBytesReturned*8;

            //Compare to origional signal
            int64_t uncodedBitErrorsPkt = bitErrors(uncodedPkt, decodedBytes, ENCODE_PKT_BYTE_LEN);
            decodedBitErrors += uncodedBitErrorsPkt;
        }

        double decodedBER = (double) decodedBitErrors/decodedBitsRecieved;
        double relativeError = fabs(expectedCodedBer[configInd] - decodedBER)/expectedCodedBer[configInd];
        printf("%6.1f | %20e  %20e  %10ld  %11ld | %18e  %18e  %10ld  %11ld  %%%6.2f\n", snr[configInd], uncodedBer[configInd], (double) codedBitErrors/codedBitsSent, codedBitErrors, codedBitsSent, expectedCodedBer[configInd], decodedBER, decodedBitErrors, decodedBitsRecieved, relativeError*100); //Note: The uncoded BER is the rate at which the transmitted bits were corrupted.  The bits sent were the coded bits.  The decoded BER is the BER after final decoding
        if(relativeError > REL_ERROR_THRESH){
            failed = true;
        }
    }

    if(failed){
        printf("Failed! Error too large (over %%%6.2f)!\n", REL_ERROR_THRESH*100);
        return 1;
    }

    printf("Success!\n");
    return 0;
}