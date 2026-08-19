#ifndef EFFECT_H
#define EFFECT_H

#include "util.hpp"
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;


// template <typename T>
class Control { // store single int for control's parameter id   

public:
    // EffectParameter<T>* parameter;

    virtual void update() = 0;
};

template <typename T> class EffectParameter {

public:
    T initialValue; 
    T value;
    const char* name;
    Control* control;

    EffectParameter() : initialValue(0), value(0), name(nullptr), control(nullptr) {};
    EffectParameter(const char* name, T initialValue) : initialValue(initialValue), value(initialValue), name(name) {}

    ParameterSetting<T> save();
    void load(ParameterSetting<T> setting);

};


class ToggleControl : public Control/* <bool> */ {

    Switch button;

public:
    int id;
    EffectParameter<bool>* parameter; // menu to set this per control

    ToggleControl() {};
    ToggleControl(dsy_gpio_pin pin, int index);
    void update();

};



class VariableControl : public Control/* <float> */ {
    int channel;
    DaisySeed* hw;

public:
    int id;
    EffectParameter<float>* parameter;

    VariableControl() {};
    VariableControl(DaisySeed* hw, AdcChannelConfig* config, dsy_gpio_pin pin, int index);

    // THESE HAVE TO BE DONE when this and all others are initialized
    // hw->adc.Init(configs, n);
    // hw->adc.Start();
    
    void update();

};


// class Filter {

//     virtual void 

// };

class CombFilter /* : Filter */ {

    CircularBuffer buffer;

    float gain;
    float depth;
    float mix;
    size_t maxSamples;

public:
    CombFilter() {}
    CombFilter(float delay, float gain);
    void apply(const float* in, float* out, size_t samples);

    void setParams(float depth, float mix);

};

class AllPassFilter /* : Filter */ {

    CircularBuffer feedbackBuffer;
    CircularBuffer inputBuffer;

    float gain;
    int maxSamples;

public:
    AllPassFilter() {}
    AllPassFilter(float delay, float gain);
    float applyOne(float in);
    void apply(const float* in, float* out, size_t samples);

    void setParams(float gain, float depth);
    void setGain(float gain);
    void offsetSamples(int samples);

};


class Effect {

public:
    // const char* name;    

    EffectParameter<bool> enabled; //= EffectParameter<bool>("Enable", false);
    bool series; // i guess more if it captures output as input (like from other effects) - if this is true, the output will be given as the in for apply

    Effect(bool series) : series(series) {}
    virtual void apply(const float* in, float* out, size_t samples) = 0;
    virtual void update() = 0;

};

// these kinds of things might be replaced by constructor defaults in the future idk how flexible i want to make it
#define MAX_LOOP_SECONDS 60
#define MAX_LOOP_SAMPLES (SAMPLE_RATE * MAX_LOOP_SECONDS)
#define MIN_LOOP_SAMPLES SAMPLE_RATE

class Looper : public Effect {

    float* buffer; // coulda used a circular buffer but its not good
    int loopSamples = 0;
    int currentLoopSample;

    bool lastRecEnabled; // this is dumb because falling and rising edge exist
    bool lastPlaybackEnabled;

public:
    EffectParameter<bool> recordingEnabled = EffectParameter<bool>("Loop Record", false);

    Looper(bool series);
    void apply(const float* in, float* out, size_t samples);
    void update();

};


#define MAX_DELAY_SECONDS 0.5f
#define DELAY_SIZE (int)(SAMPLE_RATE * MAX_DELAY_SECONDS)

class Delay : public Effect {

    CircularBuffer buffer;

public:
    EffectParameter<float> delayAmount = EffectParameter<float>("Delay Time", false);

    Delay(bool series); // might not need offset because itll be gotten

    void apply(const float* in, float* out, size_t samples);
    void update() {}
};



#define DFT_NEW_WINDOW_SAMPLES 16384 // maybe increase this (already pretty big)
#define DFT_OVERLAP_SAMPLES DFT_NEW_WINDOW_SAMPLES 
#define DFT_WINDOW_SAMPLES (DFT_NEW_WINDOW_SAMPLES + DFT_OVERLAP_SAMPLES)

class Tuner : public Effect {

    float* dftTimeBufferA;
    float* dftTimeBufferB;

    bool usingBufferA = false;
    int currentDftWindowSamples = 0;
    std::complex<float> dftFrequencyBuffer[DFT_WINDOW_SAMPLES]; // maybe zero pad, idk. (f = k*fs/N) // TODO: sdram_alloc this (i think)
    bool newDftReady = false;

public:
    float maxFrequency = 0;

    Tuner(bool series/* , size_t windowSize */);

    void apply(const float* in, float* out, size_t samples);
    void update();

};

// referenced https://medium.com/the-seekers-project/coding-a-basic-reverb-algorithm-part-2-an-introduction-to-audio-programming-4db79dd4e325
#define REVERB_NUM_COMB_FILTERS 4
#define REVERB_NUM_AP_FILTERS 2
class Reverb : public Effect {

    CombFilter combFilters[REVERB_NUM_COMB_FILTERS]; // potential optimization: these can share a delay line / circular buffer i think
    AllPassFilter allPassFilters[REVERB_NUM_AP_FILTERS];

public:
    EffectParameter<float> depth = EffectParameter<float>("Reverb Depth", 0.5f);
    EffectParameter<float> mix = EffectParameter<float>("Reverb Mix", 1);
    EffectParameter<float> gain = EffectParameter<float>("Reverb Gain", 0.7f);

    Reverb(bool series);

    void apply(const float* in, float* out, size_t samples);
    void update();

};


// referenced https://upcommons.upc.edu/server/api/core/bitstreams/2324a4c3-e11c-42fe-b50a-bbefaa6fb2d0/content
class Chorus : public Effect {

    daisysp::Oscillator osc;
    CircularBuffer buffer;
    size_t maxDelaySamples;
    size_t delaySamples;

public:
    EffectParameter<float> frequency = EffectParameter<float>("Chorus Rate", 0.3f);
    EffectParameter<float> depth = EffectParameter<float>("Chorus Depth", 0.4f);
    EffectParameter<float> mix = EffectParameter<float>("Chorus Mix", 1); 

    Chorus(bool series, float maxDelay); // todo: waveform should be changed with buttons / rotary encoder (int parameter)

    void apply(const float* in, float* out, size_t samples);
    void update();


};

#define CRUSH_AMOUNT_SCALE 200 // 1000.0f
class BitCrusher : public Effect {

    float heldValue = 0;
    int heldSamples = 0;

public:
    EffectParameter<float> amount = EffectParameter<float>("Bit Crush Amnt", 0.8f);

    BitCrusher(bool series);

    void apply(const float* in, float* out, size_t samples);
    void update() {}

};


class AllPassFilter2 {

    float inputSample = 0;
    float outputSample = 0;
    
public:
    float coefficient = 0;

    AllPassFilter2() {};
    float process(float in);

};


#define PHASER_NUM_AP_FILTERS 12
class Phaser : public Effect {

    // daisysp::Oscillator osc;
    float oscPhase = 0;
    float heldSample = 0;

    AllPassFilter2 allPassFilters[PHASER_NUM_AP_FILTERS];

public:
    EffectParameter<float> rate = EffectParameter<float>("Phaser Rate", 0.1f);
    EffectParameter<float> depth = EffectParameter<float>("Phaser Depth", 0.7f);
    EffectParameter<float> feedback = EffectParameter<float>("Phaser Feedback", 0.5f);

    Phaser(bool series);    

    void apply(const float* in, float* out, size_t samples);
    void update() {};

};

class Metronome : public Effect {

    int waitCount = 0;
    int playbackSample = 0;
    int downsampleCount = 0;
    bool shouldPlay = false;

public:

    int bpm = 100;
    EffectParameter<float> volume = EffectParameter<float>("Metronome Mix", 0.5f);

    Metronome();

    void apply(const float* in, float* out, size_t samples);
    void update() {};

};



#endif
