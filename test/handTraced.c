//Use hand written example from PPT (expand to 8 bits)

//Run single itterations of the decoder and check the state in between coded segments

#include "convEncode.h"
#include "viterbiDecoder.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char* argv[]){
    printf("=============== Hand Traced Test ===============\n");
    
    printf("Params:\n");
    printf("\tk:    %d\n", k);
    printf("\tK:    %d\n", K);
    printf("\tn:    %d\n", n);
    for(int i = 0; i<n; i++){
        printf("\t\tg[%d]=%lo\n", i, g[i]);
    }
    printf("\tRate: %f\n", Rc);
    printf("\tNum States: %lu\n", NUM_STATES);

    printf("********** Convolutional Encoder Test **********\n");

    convEncoderState_t convEncState;
    resetConvEncoder(&convEncState);
    initConvEncoder(&convEncState);

    uint8_t uncoded[1] = {0b01101000};

    uint8_t codedSegments[8/k+S];

    int codedSegsReturned = convEnc(&convEncState, uncoded, codedSegments, 1, true);

    printf("Encoded: 0x%x, Got %d coded segments\n", uncoded[0], codedSegsReturned);
    assert(codedSegsReturned == 8/k+S);

    uint8_t expectedCoded[8/k+S] = {0b00, 0b11, 0b00, 0b10, 0b10, 0b11, 0b01, 0b00, 0b00, 0b00};

    bool failed = false;
    for(int i = 0; i<(8/k+S); i++){
        printf("\tCoded[%2d]: Got 0x%x, Expected 0x%x\n", i, codedSegments[i], expectedCoded[i]);
        if(codedSegments[i] != expectedCoded[i]){
            failed = true;
        }
    }
    assert(!failed);

    printf("************* Viterbi Decoder Test *************\n");
    viterbiHardState_t viterbiState;
    resetViterbiDecoderHard(&viterbiState);
    viterbiInit(&viterbiState);
    viterbiConfigCheck();

    uint8_t corruptedCoded[8/k+S] = {0b01, 0b11, 0b01, 0b10, 0b10, 0b11, 0b01, 0b00, 0b00, 0b00};
    uint8_t decoded[1];
    int decodedBytesReturned = viterbiDecoderHard(&viterbiState, corruptedCoded, decoded, 8/k+S, true);

    printf("Decoded (String Order):");
    for(int i = 0; i<(8/k+S); i++){
        printf(" 0x%x", corruptedCoded[i]);
    }
    printf(", Got %d decoded byte(s)\n", decodedBytesReturned);
    assert(decodedBytesReturned == 1);

    uint8_t expected = 0b01101000;
    printf("Decoded 0x%x, Expected 0x%x\n", decoded[0], expected);
    assert(decoded[0] == expected);

    printf("++++ Test Passed! ++++\n");
    return 0;
}