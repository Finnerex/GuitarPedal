#include "effect.hpp"


// ToggleControl

ToggleControl::ToggleControl(dsy_gpio_pin pin) {
    button.Init(pin, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_NORMAL, Switch::PULL_NONE);
}

void ToggleControl::update() {
    button.Debounce();
    parameter->value = button.Pressed();
}   


// VariableControl

VariableControl::VariableControl(DaisySeed* hw, AdcChannelConfig* config, dsy_gpio_pin pin) {
        config->InitSingle(pin);
        channel = numChannels++;
    }

void VariableControl::update() {

    float currentVal = hw->adc.GetFloat(channel);

    if (std::abs(currentVal - parameter->value) >= 0.01f) // has changed by > 1 percent
        parameter->value = currentVal;
    
}


// Looper

void Looper::apply(const float* in, float* out, int samples) {

    // so the enabled thing cant go outside for this to work
    if (recordingEnabled.value) {

        if (loopSamples + samples <= maxSize) {
            memcpy(buffer + loopSamples, in, sizeof(float) * samples);
            loopSamples += samples;
        }

    } else if (enabled.value) {

        for (int i = 0; i < samples; i++) { 
            out[i] += buffer[currentLoopSample + i];
        }

        currentLoopSample += samples;
        currentLoopSample %= loopSamples;
    } 

}

void Looper::update() {
    if (recordingEnabled.value && !lastRecEnabled) { 
        loopSamples = 0;
    }

    if ((enabled.value && !lastPlaybackEnabled) || (!recordingEnabled.value && lastRecEnabled)) {
        currentLoopSample = 0;
    }

    lastPlaybackEnabled = enabled.value;
    lastRecEnabled = recordingEnabled.value;
}


// Delay

void Delay::apply(const float* in, float* out, int samples) {

    if (!enabled.value) return; // dumb that i have to put this on all of them

    buffer.setOffset(delayAmount.value * buffer.size); // ie percent of maximum (value between 0 and 1)

    for (int i = 0; i < samples; i++) {
        out[i] += buffer.readNext();
        buffer.writeNext(in[i]);
    }
    
}