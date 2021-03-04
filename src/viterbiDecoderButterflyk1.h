#ifndef _VITERBI_DECODER_BUTTERFLYk1_H_
#define _VITERBI_DECODER_BUTTERFLYk1_H_

#include "viterbiDecoder.h"

int viterbiDecoderHardButterflyk1(viterbiHardState_t* restrict state, uint8_t* restrict codedSegments, uint8_t* restrict uncoded, int segmentsIn, bool last);

void viterbiInitButterflyk1(viterbiHardState_t* state);

void resetViterbiDecoderHardButterflyk1(viterbiHardState_t* state);

#endif