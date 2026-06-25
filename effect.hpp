#ifndef EFFECT_H
#define EFFECT_H

#include "util.hpp"
#include "daisy_seed.h"

using namespace daisy;


// class Control {
//     virtual void update() = 0;
// };

template <typename T> struct EffectParameter {
    T value;
    const char* name;

    EffectParameter() {};
    EffectParameter(const char* name) : name(name) {};
};


class ToggleControl /* : Control */ {

    Switch button;

public:
    EffectParameter<bool>* parameter; // menu to set this per control

    ToggleControl(dsy_gpio_pin pin);
    void update();
};


class VariableControl /* : Control */ {
    int channel;
    DaisySeed* hw;

public:
    EffectParameter<float>* parameter;

    VariableControl(DaisySeed* hw, AdcChannelConfig* config, dsy_gpio_pin pin);

    // THESE HAVE TO BE DONE when this and all others are initialized
    // hw->adc.Init(configs, n);
    // hw->adc.Start();
    
    void update();

};

class Effect {

public:
    
    EffectParameter<bool> enabled = EffectParameter<bool>("Enable");
    bool series; // i guess more if it captures output as input (like from other effects) - if this is true, the output will be given as the in for apply

    Effect(bool series) : series(series) {}
    virtual void apply(const float* in, float* out, size_t samples) = 0;
    virtual void update() = 0;

};

class Looper : public Effect {

    float* buffer; // coulda used a circular buffer but its not good
    int loopSamples;
    int currentLoopSample;
    size_t maxSize;
    bool lastRecEnabled; // this is dumb because falling and rising edge exist
    bool lastPlaybackEnabled;

public:
    EffectParameter<bool> recordingEnabled = EffectParameter<bool>("Enable Recording");

    Looper(bool series, float* buffer, size_t size) : Effect(series), buffer(buffer), maxSize(size) {}
    void apply(const float* in, float* out, size_t samples);
    void update();

};

class Delay : public Effect {

    CircularBuffer buffer;

public:
    EffectParameter<float> delayAmount = EffectParameter<float>("Delay Amount");

    Delay(bool series, float* buffer, size_t size, size_t offset) : Effect(series), buffer(buffer, size, offset) {} // might not need offset because itll be gotten

    void apply(const float* in, float* out, size_t samples);
    void update() {}
};



#define DFT_NEW_WINDOW_SAMPLES 16384 //(int)(0.5f * SAMPLE_RATE) // (not right now) quarter second updates for now
#define DFT_OVERLAP_SAMPLES DFT_NEW_WINDOW_SAMPLES //(int)(0.7f * DFT_NEW_WINDOW_SAMPLES) // (not right now) half overlap (or is it a third)
#define DFT_WINDOW_SAMPLES (DFT_NEW_WINDOW_SAMPLES + DFT_OVERLAP_SAMPLES)

class Tuner : public Effect {

    float* dftTimeBufferA;
    float* dftTimeBufferB;

    bool usingBufferA = false;
    int currentDftWindowSamples = 0;
    std::complex<float> dftFrequencyBuffer[DFT_WINDOW_SAMPLES]; // maybe zero pad, idk. (f = k*fs/N)
    bool newDftReady = false;

public:
    float maxFrequency = 0;

    Tuner(bool series, size_t windowSize);

    void apply(const float* in, float* out, size_t samples);
    void update();

};


// class Filter {

//     virtual void 

// };

// class CombFilter /* : Filter */ {

//     CircularBuffer buffer;

//     float gain;

//     CombFilter(float* buffer, float delay, float gain) : buffer(buffer, ), gain(gain) {}
//     void apply(float* in, float* out, size_t samples);

// };

// class AllPassFilter /* : Filter */ {

// };



#endif