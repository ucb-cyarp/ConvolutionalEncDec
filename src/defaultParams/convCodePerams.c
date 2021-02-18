#include "convCodePerams.h"

//Note, starting with a 0 indecates an octal
//Note, in the Proakis convention, the generators are big endian with the MSB representing the most recent input bit in the encoder
//internally, these generators will be converted to little endian representations
const uint64_t g[n] = {0113, 0171};