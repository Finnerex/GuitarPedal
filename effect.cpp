#include "util.hpp"
#include "daisy_seed.h"

using namespace daisy;


// class Control {
//     virtual void update() = 0;
// };

class ToggleControl /* : Control */ {

    Switch button;

public:
    bool* value; // have some kind of menu to set this per effect, also maybe this should be a ptr to an EffectParameter

    ToggleControl(dsy_gpio_pin pin) {
        button.Init(pin, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_NORMAL, Switch::PULL_NONE);
    }

    void update() {
        button.Debounce();
        *value = button.Pressed();
    }

};

class VariableControl /* : Control */ {
    int channel;
    DaisySeed* hw;

public:
    float* value;
    static int numChannels;

    VariableControl(DaisySeed* hw, AdcChannelConfig* config, dsy_gpio_pin pin) {
        config->InitSingle(pin);
        channel = numChannels++;
    }

    // THESE HAVE TO BE DONE when this and all others are initialized
    // hw->adc.Init(configs, n);
    // hw->adc.Start();

    void update() {

        float currentVal = hw->adc.GetFloat(channel);

        if (std::abs(currentVal - *value) >= 0.01f) // has changed by > 1 percent
            *value = currentVal;
        
    }

};

template <typename T> struct EffectParameter {
    T value;
    const char* name;

    EffectParameter() {}
    EffectParameter(const char* name) : name(name) {}
};


class Effect {

public:
    // hella wasted space but can be changed later i guess
    EffectParameter<bool> boolParams[2];
    size_t numBoolParams;
    
    EffectParameter<float> floatParams[2];
    size_t numFloatParams;
    
    // bool enabled;
    bool series; // i guess more if it captures output as input (like from other effects) - if this is true, the output will be given as the in for apply

    Effect(bool series) : series(series) {
        boolParams[0] = EffectParameter<bool>("Enabled");
        numBoolParams = 1;
    }

    virtual void apply(float* in, float* out, int samples) = 0;

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


class Looper : Effect {

    float* buffer; // coulda used a circular buffer but its not good
    int loopSamples;
    int currentLoopSample;
    size_t maxSize;

    bool lastRecEnabled; // this is dumb because falling and rising edge exist
    bool lastPlaybackEnabled;

    Looper(bool enabled, bool series, float* buffer, size_t size) : Effect(series), buffer(buffer), maxSize(size) {
        boolParams[1] = EffectParameter<bool>("Recording Enabled");
        numBoolParams = 2;
    }

    void apply(float* in, float* out, int samples) {

        // this probably shouldnt be done in the audio callback bc it doesnt have anything to do with that
        if (boolParams[1].value && !lastRecEnabled) { // so the enabled thing cant go outside for this to wokr
            loopSamples = 0;
        }

        if ((boolParams[0].value && !lastPlaybackEnabled) || (!boolParams[1].value && lastRecEnabled)) {
            currentLoopSample = 0;
        }

        lastPlaybackEnabled = boolParams[0].value;
        lastRecEnabled = boolParams[1].value;



        if (boolParams[1].value) {

            if (loopSamples + samples <= maxSize) {
                memcpy(buffer + loopSamples, in, sizeof(float) * samples);
                loopSamples += samples;
            }

        } else if (boolParams[0].value) {

            for (int i = 0; i < samples; i++) { 
                out[i] += buffer[currentLoopSample + i];
            }

            currentLoopSample += samples;
            currentLoopSample %= loopSamples;
        } 

    }

};

class Delay : Effect {

    CircularBuffer buffer;

    Delay(bool enabled, bool series, float* buffer, size_t size, size_t offset) : Effect(series), buffer(buffer, size, offset) {
        floatParams[0] = EffectParameter<float>("Delay Amount");
        numFloatParams = 1;
    }

    void apply(float* in, float* out, int samples) {

        if (!floatParams[0].value) return; // dumb that i have to put this on all of them

        buffer.setOffset(floatParams[0].value * buffer.size); // ie percent of maximum (value between 0 and 1)

        for (int i = 0; i < samples; i++) {
            out[i] += buffer.readNext();
            buffer.writeNext(in[i]);
        }
        
    }

};
