#ifndef _CONV_ENCODE_H_
#define _CONV_ENCODE_H_

#include "convCodeParams.h"
#include "convHelpers.h"
#include <stdbool.h>

//TODO: Create State which includes a circular buffer and the code.
//TODO: Use circular buffer and dot product techniques from Laminar here

//TODO: Test if dot product or supplying a list of indexes is better

//TODO: Test if unpacked bits or packed bits are better for the circular buffer (will be trying unpacked first)

//TODO: Implement encoder init and reset

typedef struct{
    //Circular buffer will be double the length technicall required.  This allows us to always
    //present stride 1 access to the dot product operations.  This requires redundant writes
    //to the two halfs of the buffer.
    //Because this is a tapped delay line and not a FIFO, only one cursor index is maintained.
    //The buffer currently has the most recent element first with the cursor decrementing
    uint8_t tappedDelay[2*k*K]; //This includes the full K
    int tappedDelayCursor;
    
    //The polynomials are stored as byte arrays, one array per polynomial
    //In the C semantics, a single row of a 2D array is stored contiguously (Row Major).
    //ie, array[i][j] is equivalent to *(array+(i*b)+j) where b is the number of columns
    
    //As such each polynomial is a row in this 2D array with the number of colums being the
    //number of bits in the shift register
    uint8_t polynomials[n][k*K]; //This includes the full K

    uint8_t remainingUncoded; //Used for cases when 8%k != 0
    uint8_t remainingUncodedCount; //Details the number of bits that are left in remainingUncoded
} convEncoderState_t;

void resetConvEncoder(convEncoderState_t* state);

/**
 * @brief Initializes the tapped delay with the requested state
 * @param tappedDelayStateRequested the requested state (little endian)
 * 
 * TODO: Change if big endian tapped delay operation used in the future
 */
void setConvEncoderState(convEncoderState_t* state, int tappedDelayStateRequested);

/**
 * @brief Initializes the polynomial arrays in the convolutional encoder state
 */
void initConvEncoder(convEncoderState_t* state);

/**
 * @brief Inserts a new input into the tappedDelay but does not adjust the cursor
 */
void tappedDelayInsert(uint8_t* tappedDelay, int tappedDelayCursor, uint8_t input);

/**
 * @brief Decements the cursor and wraps it arround as nessisary
 */
void tappedDelayStep(int* tappedDelayCursor);

/**
 * @brief Convolutionally encode packed data and emits coded segments (k bits each).  The data is sent in big endian order (the MSb is encoded first).
 * 
 * @note For scenarios where k>1, big endian order is used for the bits within the subword (the MSb is shifted in first followed by the next bits)
 *       This allows the k bit value value to simply be appended to the traceback
 * 
 * TODO: Update if little endian bit ordering durring transmission is requested.
 *       Since the shift register (and the associated state indexes) assume shifting into the LSb, big endian transmission makes it easier to handle cases with k>1
 *       (values can be shifted in the same order as before)
 * 
 *       While the endian changes could be accomodated with changes to the unpacking and packing logic, moving to shifting into the MSb of the shift register
 *       could make it easier to handle cases with
 * 
 * @warning The coded array provided must be large enough to accomodate the maximum number of coded segments that could result from the encoding all of the provided bytes.  If the rate is 1/2, the codedSegments array must be twice the length of the uncoded array.  It also must be long enough to accomodate any padding (K additional coded segments)
 * 
 * @note If 8%k != 0, the message length (in bits) must be devisible by k and 8.  ie. if k=3, the message must be in a multiple of 3 bytes
 * 
 * @param uncoded an array of bytes to be encoded.  They are encoded in acending array index (like a string, sending a byte at a time from the lowest index).  Each byte is sent in big endian bit order.  
 * @param codedSegemets an array of coded segments.  Each segment is n bits long.  The index is in assending send order with the 0'th array element transmitted first
 * @param bytesIn the number of uncoded bytes sent to this function.  This should be equal to the block size unless it is the end of the message.
 * @param last indicates that the passed uncoded data is the last in the packet and that the final padding should occure
 * 
 * @return the number of coded segements written
 * 
 */
int convEnc(convEncoderState_t* state, uint8_t* uncoded, uint8_t* codedSegments, int bytesIn, bool last);

/**
 * @brief Computes the output from the encoder based on the current state
 * 
 * Requires the cursor to be at the most recently shifted in element and not to the next location.
 * The cursor should be moved after this
 * 
 * @returns the output from the encoder
 */
uint8_t computeEncOutputSegment(convEncoderState_t* state);

/**
 * Encodes a single k bit segment.  Useful for determining the expected values for different edges in the trellis
 * 
 * @param bitsToShiftIn k bits to shift into the shift register.  Will be shifted in big endian bit order with the MSB being shifted in first and the LSb being shifted in last
 * 
 * TODO: Change if endianess of tapped delay is changed
 * 
 * @returns the coded segment corresponding to the single input
 */
int convEncOneInput(convEncoderState_t* state, uint8_t bitsToShiftIn);


#endif