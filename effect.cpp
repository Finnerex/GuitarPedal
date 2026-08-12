#include "effect.hpp"


// ToggleControl

ToggleControl::ToggleControl(dsy_gpio_pin pin, int index) : id(index) {
    button.Init(pin, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_NORMAL, Switch::PULL_NONE);
}

void ToggleControl::update() {

    if (parameter == nullptr) return;

    button.Debounce();
    parameter->value = button.Pressed();
}   

// VariableControl
int numChannels; // would love to make this static but its dumb and doesnt work

VariableControl::VariableControl(DaisySeed* hw, AdcChannelConfig* config, dsy_gpio_pin pin, int index) : id(index) {
    config->InitSingle(pin);
    channel = numChannels++;
}

void VariableControl::update() {

    if (parameter == nullptr) return;

    float currentVal = hw->adc.GetFloat(channel);

    if (std::abs(currentVal - parameter->value) >= 0.005f) // has changed by > 0.5 percent
        parameter->value = currentVal;
    
}

// Effect Parameter

template <typename T> // TODO: REMOVE AND GO BACK TO JUST CONSTRUCTOR
void EffectParameter<T>::Init(const char* name, T initialValue) {
    value = initialValue;
    this->name = name;
}


template <>
ParameterSetting<bool> EffectParameter<bool>::save() {

    if (control != nullptr) {
        return { value, (static_cast<ToggleControl*>(control))->id };
    }

    return { value, -1 };
}

// excellent duplicated code!!
template <>
ParameterSetting<float> EffectParameter<float>::save() {

    if (control != nullptr) {
        return { value, (static_cast<VariableControl*>(control))->id };
    }

    return { value, -1 };
}


template <>
void EffectParameter<bool>::load(ParameterSetting<bool> setting) {

    int cId = setting.controlId;
    if (cId >= 0 && cId < NUM_TOGGLE_CONTROLS) {
        control = buttons[cId];
        buttons[cId]->parameter = this;
    }

    value = setting.value;
}

// currently these are held at zero and dont do anything idk why
template <>
void EffectParameter<float>::load(ParameterSetting<float> setting) {

    int cId = setting.controlId;
    if (cId >= 0 && cId < NUM_VARIABLE_CONTROLS) {
        control = potentiometers[cId];
        potentiometers[cId]->parameter = this;
    }

    value = std::clamp(setting.value, 0.f, 1.f);
}


// CombFilter
// note: maybe include this as its own effect because its kinda neat
CombFilter::CombFilter(float delay, float gain) : gain(gain), depth(1), mix(1) {
    if (this->gain > 0.99) this->gain = 0.99;

    const size_t samples = delay * SAMPLE_RATE;
    buffer = CircularBuffer((float*)sdram_alloc(samples * sizeof(float), true), samples, samples);

    max_samples = samples;

}

void CombFilter::apply(const float* in, float* out, size_t samples) {

    for (size_t i = 0; i < samples; i++) {

        float nextRead = buffer.readNext();
        buffer.writeNext(in[i] + nextRead * gain * depth);
        out[i] += nextRead * mix;

    }

}

void CombFilter::setParams(float depth, float mix) {
    this->depth = depth;
    this->mix = mix;
    buffer.setWriteOffset(max_samples * depth);
}


// AllPassFilter

AllPassFilter::AllPassFilter(float delay, float gain) : gain(gain) {
    if (this->gain > 0.99) this->gain = 0.99;

    const size_t samples = delay * SAMPLE_RATE;

    feedbackBuffer = CircularBuffer((float*)sdram_alloc(samples * sizeof(float), true), samples, samples);
    inputBuffer = CircularBuffer((float*)sdram_alloc(samples * sizeof(float), true), samples, samples);

}

void AllPassFilter::apply(const float* in, float* out, size_t samples) {

    for (size_t i = 0; i < samples; i++) {

        float nextRead = feedbackBuffer.readNext();
        
        feedbackBuffer.writeNext(-gain * in[i] + inputBuffer.readNext() + nextRead * gain);
        inputBuffer.writeNext(in[i]);

        out[i] += nextRead;

    }

}

// Looper

Looper::Looper(bool series) : Effect(series), buffer((float*)sdram_alloc(MAX_LOOP_SAMPLES)) {
    enabled.Init("Loop Playback", false);
}

void Looper::apply(const float* in, float* out, size_t samples) {

    // so the enabled thing cant go outside for this to work
    if (recordingEnabled.value) {

        if (loopSamples + samples <= MAX_LOOP_SAMPLES) {
            memcpy(buffer + loopSamples, in, sizeof(float) * samples);
            loopSamples += samples;
        }

    } else if (enabled.value && loopSamples > MIN_LOOP_SAMPLES) { // at least one second for playback to be allowed

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

// struct LooperSettings {

// }



// Delay

Delay::Delay(bool series) : Effect(series) {
    buffer = CircularBuffer((float*)sdram_alloc(DELAY_SIZE * sizeof(float), true), DELAY_SIZE, 0);
    enabled.Init("> Delay", false);
}

void Delay::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return; // dumb that i have to put this on all of them

    buffer.setWriteOffset(delayAmount.value * buffer.size); // ie percent of maximum (value between 0 and 1)

    for (size_t i = 0; i < samples; i++) {
        buffer.writeNext(in[i]);
        out[i] += buffer.readNext();
    }
    
}


// Tuner

// double buffer: one to copy to and one to process

Tuner::Tuner(bool series/* , size_t windowSize = DFT_WINDOW_SAMPLES */) : Effect(series) {
    dftTimeBufferA = (float*)sdram_alloc(DFT_WINDOW_SAMPLES * sizeof(float));
    dftTimeBufferB = (float*)sdram_alloc(DFT_WINDOW_SAMPLES * sizeof(float));
    enabled.Init("> Tuner", false);
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

// Reverb

Reverb::Reverb(bool series) : Effect(false) { // maybe change apply so it copys input, otherise should be first in chain
    
    combFilters[0] = CombFilter(0.1f, 0.742f);
    combFilters[1] = CombFilter(0.1041f, 0.733f);
    combFilters[2] = CombFilter(0.1125f, 0.715f);
    combFilters[3] = CombFilter(0.1209f, 0.697f);

    allPassFilters[0] = AllPassFilter(0.0219f, 0.7f);
    allPassFilters[1] = AllPassFilter(0.007021f, 0.7f);

    enabled.Init("> Reverb", false);

}

void Reverb::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return;
    
    for (size_t i = 0; i < NUM_COMB_FILTERS; i++) {
        combFilters[i].apply(in, out, samples);
    }

    for (size_t i = 0; i < NUM_AP_FILTERS; i++) {
        allPassFilters[i].apply(out, out, samples); // out -> out series application
    }

}

void Reverb::update() {

    for (int i = 0; i < NUM_COMB_FILTERS; i++) {
        combFilters[i].setParams(depth.value, mix.value);
    }

}


// Chorus

Chorus::Chorus(bool series, float maxDelay) : Effect(series) {
    osc.Init(SAMPLE_RATE);
    osc.SetWaveform(daisysp::Oscillator::WAVE_SIN);
    osc.SetAmp(0.5f);

    maxDelaySamples = maxDelay * SAMPLE_RATE;
    buffer = CircularBuffer((float*)sdram_alloc(maxDelaySamples * sizeof(float), true), maxDelaySamples, 0); // idk if zeroing is needed here but i got it

    delaySamples = maxDelaySamples;

    enabled.Init("> Chorus", false);
    
}

void Chorus::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return;

    for (size_t i = 0; i < samples; i++) {
        buffer.setReadOffset((osc.Process() + 0.5f) * delaySamples); // this might have to change the write position and not the read position

        buffer.writeNext(in[i]);
        out[i] += buffer.readNext() * mix.value;
    }

}

void Chorus::update() {

    osc.SetFreq(15 * frequency.value + 0.1f); // maybe make this logarithmic
    delaySamples = maxDelaySamples * depth.value;

}


// BitCrusher

BitCrusher::BitCrusher(bool series) : Effect(series) {
    enabled.Init("> Bit Crusher", false);
}

void BitCrusher::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return;
    
    // float crushAmount = (amount.value + 1) * 1200;
    // int mask = ~((int)(amount.value * CRUSH_AMOUNT_SCALE));

    // TODO: 32 magic number moment
    int downsampleValue = amount.value * 32 + 2; // could go in update

    for (size_t i = 0; i < samples; i++) {
        // TODO: figure out equals / += or series or whatever
        
        if (heldSamples <= 0) {
            heldSamples = downsampleValue;
            heldValue = in[i];
        }

        out[i] = heldValue;
        heldSamples--;

        // out[i] = (((int)(in[i] * (INT16_MAX))) & (mask)) / (float)(INT16_MAX);
        // out[i] = ((int)(in[i] * crushAmount)) / crushAmount;

    }

}


