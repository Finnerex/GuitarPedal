#ifndef UTIL_H
#define UTIL_H

#include <cmath>
#include <complex>

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
