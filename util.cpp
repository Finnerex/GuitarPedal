#include "util.hpp"
#include <algorithm>

using namespace std::complex_literals;

#define My_PI 3.14159265358979323846f // like i guess bro


DSY_SDRAM_BSS uint8_t sdramBuffer[MEMORY_SIZE];
size_t nextOffset = 0;

void* sdram_alloc(size_t size) {
    if (nextOffset + size >= MEMORY_SIZE)
        return nullptr;

    void* ptr = sdramBuffer + nextOffset;
    nextOffset += size;
    
    return ptr;
}



// thanks wikipedia
void ditfft2(float* x, int N, int s, std::complex<float>* X)
{

    if (N == 1){
        X[0] = x[0];
        return;
    }

    ditfft2(x, N/2, 2 * s, X);
    ditfft2(x + s, N/2, 2 * s, X + N/2);

    for (int k = 0; k < N/2; k++) {
        std::complex<float> p = X[k];
        std::complex<float> q = exp(-2 * (float)My_PI * 1if * (float)k / (float)N) * X[k + N / 2];
        X[k] = p + q;
        X[k + N / 2] = p - q;
    }

}

const char* notes[] = { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };
#define C0 16.3515978313f

// TODO: wrong octave still
// returns error from closest note (+- 0.5 from note usually)
float frequencyToNote(float freq, const char** note, int* octave)
{

    // idk i think i did this weird
    float n = 12 * std::log2f(freq / C0);
    float tone = fmodf(n - 9, 12);
    int noteNumber = std::abs((int)roundf(tone)) % 12;

    *note = notes[noteNumber];
    *octave = roundf((n - 5) / 12);

    return tone - roundf(tone);

}

// bloatware / premature abstraction / idrc / idrk

CircularBuffer::CircularBuffer(float* buffer, size_t size, size_t offset) : buffer(buffer), writePos(offset), readPos(0), size(size) {}
    // CircularBuffer(float* buffer, size_t size, size_t readPos, size_t writePos) : buffer(buffer), size(size), readPos(readPos), writePos(writePos) {}

void CircularBuffer::setOffset(size_t offset) {
    writePos = (readPos + offset) % size;
}

float CircularBuffer::readNext() {
    float f = buffer[readPos];
    readPos++;

    if (readPos >= size) // this might be faster than mod idk if it matters tho
        readPos = 0;
    
    return f;
}

void CircularBuffer::writeNext(float f) {
    buffer[writePos] = f;
    writePos++;

    if (writePos >= size)
        writePos = 0;
}

void CircularBuffer::reset() {
    writePos = 0; 
    readPos = 0;
}
