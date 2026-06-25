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
int numChannels; // would love to make this static but its dumb and doesnt work

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

void Looper::apply(const float* in, float* out, size_t samples) {

    // so the enabled thing cant go outside for this to work
    if (recordingEnabled.value) {

        if (loopSamples + samples <= maxSize) {
            memcpy(buffer + loopSamples, in, sizeof(float) * samples);
            loopSamples += samples;
        }

    } else if (enabled.value) {

        for (size_t i = 0; i < samples; i++) { 
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

void Delay::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return; // dumb that i have to put this on all of them

    buffer.setOffset(delayAmount.value * buffer.size); // ie percent of maximum (value between 0 and 1)

    for (size_t i = 0; i < samples; i++) {
        buffer.writeNext(in[i]);
        out[i] += buffer.readNext();
    }
    
}


// Tuner

// double buffer: one to copy to and one to process
// lowk fahs with the encapulated nature, would be nice to find a better solution
// so maybe i do some kind of sdram allocator, probably nothing fancy just linear, permanent allocs
// float DSY_SDRAM_BSS dftTimeBufferA[DFT_WINDOW_SAMPLES];
// float DSY_SDRAM_BSS dftTimeBufferB[DFT_WINDOW_SAMPLES];

Tuner::Tuner(bool series/* , size_t windowSize = DFT_WINDOW_SAMPLES */) : Effect(series) {
    dftTimeBufferA = (float*)sdram_alloc(windowSize * sizeof(float));
    dftTimeBufferB = (float*)sdram_alloc(windowSize * sizeof(float));
}

void Tuner::apply(const float *in, float *out, size_t samples) {

    if (!enabled.value) return;

    if (usingBufferA)
        memcpy(&dftTimeBufferA[currentDftWindowSamples], in, samples * sizeof(float));
    else
        memcpy(&dftTimeBufferB[currentDftWindowSamples], in, samples * sizeof(float));

    currentDftWindowSamples += samples;

    if (currentDftWindowSamples >= DFT_WINDOW_SAMPLES) { // collected enough for 1 dft
        newDftReady = true;
        
        if (usingBufferA)
            memcpy(dftTimeBufferB, &dftTimeBufferA[DFT_NEW_WINDOW_SAMPLES], DFT_OVERLAP_SAMPLES * sizeof(float));
        else 
            memcpy(dftTimeBufferA, &dftTimeBufferB[DFT_NEW_WINDOW_SAMPLES], DFT_OVERLAP_SAMPLES * sizeof(float));

        currentDftWindowSamples = DFT_OVERLAP_SAMPLES;
        
        usingBufferA = !usingBufferA;
    }
}

void Tuner::update() {

    if (!(newDftReady && enabled.value)) return; // hopefully this happens between buffer swaps - this isnt completely safe because the buffer could swap twice during this run i think and mess things up

    newDftReady = false;

    // run dft
    if (usingBufferA)
        ditfft2(dftTimeBufferB, DFT_WINDOW_SAMPLES, 1, dftFrequencyBuffer);
    else 
        ditfft2(dftTimeBufferA, DFT_WINDOW_SAMPLES, 1, dftFrequencyBuffer);

    float maxMagnitude = 0;
    int maxIdx = 0;

    for (int i = 0; i < DFT_WINDOW_SAMPLES / 2 + 1; i++) { // only half because it should be circular symmetric (real input signal)
        float mag = std::abs(dftFrequencyBuffer[i]);

        if (mag > maxMagnitude) {
            maxMagnitude = mag;
            maxIdx = i;
        }
    }

    maxFrequency = maxIdx * SAMPLE_RATE / (float)DFT_WINDOW_SAMPLES;  // k * fs / N

}


// CombFilter

// void CombFilter::apply(float* in, float* out, size_t samples) {



// }
