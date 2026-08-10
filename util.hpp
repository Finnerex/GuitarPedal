#ifndef UTIL_H
#define UTIL_H

#include <cmath>
#include <complex>
#include "daisy_seed.h"

#define SAMPLE_RATE 48000
#define BLOCK_SIZE 32

#define MEMORY_SIZE (60 * 1024 * 1024)

#define NUM_BOOL_PARAMETERS 7 // 7
#define NUM_FLOAT_PARAMETERS 7 // 7

#define NUM_VARIABLE_CONTROLS 4
#define NUM_TOGGLE_CONTROLS 4

// creates an unfreeable allocation of desired size at first free spot in sdram, returning a pointer to it
void* sdram_alloc(size_t size, bool zero = false);

float frequencyToNote(float freq, const char** note, int* octave);

void ditfft2(float* x, int N, int s, std::complex<float>* X);

class CircularBuffer {

    float* buffer;
    size_t writePos;
    size_t readPos;
    
public:
    size_t size;
    
    CircularBuffer() {}
    CircularBuffer(float* buffer, size_t size, size_t offset);
    // CircularBuffer(float* buffer, size_t size, size_t readPos, size_t writePos);
    void setWriteOffset(size_t offset);
    void setReadOffset(size_t offset);

    float readNext();
    void writeNext(float f);

    void reset();

};


template<typename Settings>
class Serializable {

protected:
    PersistentStorage<Settings> settings;

public:

    virtual void save() = 0;
    virtual void load() = 0;

};



#endif
