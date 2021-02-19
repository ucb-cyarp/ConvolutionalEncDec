#ifndef _CONV_HELPERS_H_
#define _CONV_HELPERS_H_

#include <stdint.h>

#define POW2(X) (1ul << (X))

void unpackBigToLittleEndian(uint8_t* unpackArray, int unpackArrayLen, uint64_t packed);
void unpackLittleToLittleEndian(uint8_t* unpackArray, int unpackArrayLen, uint64_t packed);

#endif