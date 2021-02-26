#ifndef _CONV_HELPERS_H_
#define _CONV_HELPERS_H_

#include <stdint.h>

#define POW2(X) (1ul << (X))

void unpackBigToLittleEndian(uint8_t* unpackArray, int unpackArrayLen, uint64_t packed);
void unpackLittleToLittleEndian(uint8_t* unpackArray, int unpackArrayLen, uint64_t packed);

#ifdef ARGMIN_TRACK_MIN
    #define ARGMIN_INNER_STAGE(CURRENT_NAME, PREV_NAME, ORIG, LIM) ARGMIN_INNER_STAGE_TRACEMIN(CURRENT_NAME, PREV_NAME, LIM)
    #define ARGMIN_FIRST_STAGE(CURRENT_NAME, ORIG, LIM) ARGMIN_FIRST_STAGE_TRACEMIN(CURRENT_NAME, ORIG, LIM)
    #define ARGMIN_LAST_STAGE(PREV_NAME, ORIG) ARGMIN_LAST_STAGE_TRACEMIN(PREV_NAME)
#else
    #define ARGMIN_INNER_STAGE(CURRENT_NAME, PREV_NAME, ORIG, LIM) ARGMIN_INNER_STAGE_NOTRACEMIN(CURRENT_NAME, PREV_NAME, ORIG, LIM)
    #define ARGMIN_FIRST_STAGE(CURRENT_NAME, ORIG, LIM) ARGMIN_FIRST_STAGE_NOTRACEMIN(CURRENT_NAME, ORIG, LIM)
    #define ARGMIN_LAST_STAGE(PREV_NAME, ORIG) ARGMIN_LAST_STAGE_NOTRACEMIN(PREV_NAME, ORIG)
#endif

#define ARGMIN_INNER_STAGE_TRACEMIN(CURRENT_NAME, PREV_NAME, LIM) \
    METRIC_TYPE CURRENT_NAME ## WorkingInd [LIM]; \
    METRIC_TYPE CURRENT_NAME ## MinVals [LIM]; \
    for(int i = 0; i<LIM; i++){ \
        int indA = PREV_NAME ## WorkingInd [i*2]; \
        int indB = PREV_NAME ## WorkingInd [i*2+1]; \
 \
        METRIC_TYPE valA = PREV_NAME ## MinVals [i*2]; \
        METRIC_TYPE valB = PREV_NAME ## MinVals [i*2+1]; \
 \
        if(valA <= valB){ \
            CURRENT_NAME ## WorkingInd [i] = indA; \
            CURRENT_NAME ## MinVals [i] = valA; \
        }else{ \
            CURRENT_NAME ## WorkingInd [i] = indB; \
            CURRENT_NAME ## MinVals [i] = valB; \
        } \
    }

#define ARGMIN_FIRST_STAGE_TRACEMIN(CURRENT_NAME, ORIG, LIM) \
    METRIC_TYPE CURRENT_NAME ## WorkingInd [LIM]; \
    METRIC_TYPE CURRENT_NAME ## MinVals [LIM]; \
    for(int i = 0; i<LIM; i++){ \
        int indA = i*2; \
        int indB = i*2+1; \
 \
        METRIC_TYPE valA = ORIG [i*2]; \
        METRIC_TYPE valB = ORIG [i*2+1]; \
 \
        if(valA <= valB){ \
            CURRENT_NAME ## WorkingInd [i] = indA; \
            CURRENT_NAME ## MinVals [i] = valA; \
        }else{ \
            CURRENT_NAME ## WorkingInd [i] = indB; \
            CURRENT_NAME ## MinVals [i] = valB; \
        } \
    }

#define ARGMIN_LAST_STAGE_TRACEMIN(PREV_NAME) \
    if(PREV_NAME ## MinVals [0] <= PREV_NAME ## MinVals [1]){ \
        return PREV_NAME ## WorkingInd [0]; \
    } \
    return PREV_NAME ## WorkingInd [1];


#define ARGMIN_INNER_STAGE_NOTRACEMIN(CURRENT_NAME, PREV_NAME, ORIG, LIM) \
    METRIC_TYPE CURRENT_NAME ## WorkingInd [LIM]; \
    for(int i = 0; i<LIM; i++){ \
        int indA = PREV_NAME ## WorkingInd [i*2]; \
        int indB = PREV_NAME ## WorkingInd [i*2+1]; \
 \
        METRIC_TYPE valA = ORIG [indA]; \
        METRIC_TYPE valB = ORIG [indB]; \
 \
        if(valA <= valB){ \
            CURRENT_NAME ## WorkingInd [i] = indA; \
        }else{ \
            CURRENT_NAME ## WorkingInd [i] = indB; \
        } \
    }

#define ARGMIN_FIRST_STAGE_NOTRACEMIN(CURRENT_NAME, ORIG, LIM) \
    METRIC_TYPE CURRENT_NAME ## WorkingInd [LIM]; \
    for(int i = 0; i<LIM; i++){ \
        int indA = i*2; \
        int indB = i*2+1; \
 \
        METRIC_TYPE valA = ORIG [i*2]; \
        METRIC_TYPE valB = ORIG [i*2+1]; \
 \
        if(valA <= valB){ \
            CURRENT_NAME ## WorkingInd [i] = indA; \
        }else{ \
            CURRENT_NAME ## WorkingInd [i] = indB; \
        } \
    }

#define ARGMIN_LAST_STAGE_NOTRACEMIN(PREV_NAME, ORIG) \
    if(ORIG [ PREV_NAME ## WorkingInd [0]] <= ORIG [ PREV_NAME ## WorkingInd [1] ] ){ \
        return PREV_NAME ## WorkingInd [0]; \
    } \
    return PREV_NAME ## WorkingInd [1];

#endif