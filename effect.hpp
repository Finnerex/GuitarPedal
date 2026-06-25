#ifndef EFFECT_H
#define EFFECT_H

#include "util.hpp"
#include "daisy_seed.h"

using namespace daisy;


// class Control {
//     virtual void update() = 0;
// };

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
    static int numChannels;

public:
    EffectParameter<float>* parameter;

    VariableControl(DaisySeed* hw, AdcChannelConfig* config, dsy_gpio_pin pin);

    // THESE HAVE TO BE DONE when this and all others are initialized
    // hw->adc.Init(configs, n);
    // hw->adc.Start();
    
    void update();

};

template <typename T> struct EffectParameter {
    T value;
    const char* name;

    EffectParameter() {};
    EffectParameter(const char* name) : name(name) {};
};


class Effect {

public:
    
    EffectParameter<bool> enabled = EffectParameter<bool>("Enable");
    bool series; // i guess more if it captures output as input (like from other effects) - if this is true, the output will be given as the in for apply

    Effect(bool series) : series(series) {}
    virtual void apply(const float* in, float* out, int samples) = 0;
    virtual void update() = 0;

};



// class CombFilter : Filter {

//     float delay;
//     float gain;

//     CombFilter(bool enabled, bool series, float delay, float gain) : Effect(enabled, series), delay(delay), gain(gain) {}

//     void apply(float* in, float* out, int samples) override {



//     }

// };

// class AllPassFilter : Filter {

// };


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
    void apply(const float* in, float* out, int samples);
    void update();

};

class Delay : public Effect {

    CircularBuffer buffer;

public:
    EffectParameter<float> delayAmount = EffectParameter<float>("Delay Amount");

    Delay(bool series, float* buffer, size_t size, size_t offset) : Effect(series), buffer(buffer, size, offset) {} // might not need offset because itll be gotten

    void apply(const float* in, float* out, int samples);
    void update() {}
};




#endif