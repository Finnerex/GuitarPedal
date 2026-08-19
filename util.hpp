#ifndef UTIL_H
#define UTIL_H

#include <cmath>
#include <complex>
#include "daisy_seed.h"

#define SAMPLE_RATE 48000
#define BLOCK_SIZE 16

#define MEMORY_SIZE (60 * 1024 * 1024)

#define NUM_BOOL_PARAMETERS 9
#define NUM_FLOAT_PARAMETERS 12

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

template <typename T> struct ParameterSetting {
    T value;
    int controlId;
};

struct PersistentSettings {

    ParameterSetting<bool> boolParams[NUM_BOOL_PARAMETERS];
    ParameterSetting<float> floatParams[NUM_FLOAT_PARAMETERS];

    float ledR;
    float ledG;
    float ledB;
    int metronomeBpm;

    bool operator!=(const PersistentSettings a) const {
        bool ne = false;
        
        for (int i = 0; i < NUM_BOOL_PARAMETERS; i++) {
            ne |= (boolParams[i].value != a.boolParams[i].value) || (boolParams[i].controlId != a.boolParams[i].controlId);
        }

        for (int i = 0; i < NUM_FLOAT_PARAMETERS; i++) {
            ne |= (floatParams[i].value != a.floatParams[i].value) || (floatParams[i].controlId != a.floatParams[i].controlId);
        }

        return ne;
    }

};

extern daisy::DaisySeed hw;

extern daisy::PersistentStorage<PersistentSettings> persistentData;

class VariableControl;
class ToggleControl;
template <typename T> class EffectParameter;

extern VariableControl* potentiometers[NUM_VARIABLE_CONTROLS];
extern ToggleControl* buttons[NUM_TOGGLE_CONTROLS];

extern EffectParameter<bool>* boolParams[NUM_BOOL_PARAMETERS];
extern EffectParameter<float>* floatParams[NUM_FLOAT_PARAMETERS];

#define METRONOME_CLICK_SIZE 689
extern float metronomeClick[METRONOME_CLICK_SIZE];

#endif
