#ifndef UTIL_H
#define UTIL_H

#include <cmath>
#include <complex>
#include "daisy_seed.h"

#define SAMPLE_RATE 48000
#define BLOCK_SIZE 16

#define MEMORY_SIZE (32 * 1048576)

// creates an unfreeable allocation of desired size at first free spot in sdram, returning a pointer to it
void* sdram_alloc(size_t size);

float frequencyToNote(float freq, const char** note, int* octave);

void ditfft2(float* x, int N, int s, std::complex<float>* X);

class CircularBuffer {

    float* buffer;
    size_t writePos;
    size_t readPos;
    
public:
    size_t size;
    
    CircularBuffer(float* buffer, size_t size, size_t offset);
    // CircularBuffer(float* buffer, size_t size, size_t readPos, size_t writePos);
    void setOffset(size_t offset);
    float readNext();
    void writeNext(float f);

    void reset();

};


#endif
