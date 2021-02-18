#include "convHelpers.h"

void unpackBigToLittleEndian(uint8_t* unpackArray, int unpackArrayLen, uint64_t packed){
    for(int i = unpackArrayLen-1; i>=0; i--){
        unpackArray[i] = packed % 2;
        packed = packed >> 1;
    }
}

void unpackLittleToLittleEndian(uint8_t* unpackArray, int unpackArrayLen, uint64_t packed){
    for(int i = 0; i<unpackArrayLen; i++){
        unpackArray[i] = packed % 2;
        packed = packed >> 1;
    }
}