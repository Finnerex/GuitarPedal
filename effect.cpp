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

// template <typename T>
// void EffectParameter<T>::Init(const char* name, T initialValue) {
//     value = initialValue;
//     this->name = name;
// }


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

    maxSamples = samples;

}

void CombFilter::apply(const float* in, float* out, size_t samples) {

    for (size_t i = 0; i < samples; i++) {

        float nextRead = buffer.readNext();
        buffer.writeNext(in[i] + nextRead * gain);
        out[i] += nextRead * mix;

    }

}

void CombFilter::setParams(float depth, float mix) {
    this->depth = depth;
    this->mix = mix;
    buffer.setWriteOffset(maxSamples * depth);
}


// AllPassFilter

AllPassFilter::AllPassFilter(float delay, float gain) : gain(gain) {
    if (this->gain > 0.99) this->gain = 0.99;

    const size_t samples = delay * SAMPLE_RATE;

    feedbackBuffer = CircularBuffer((float*)sdram_alloc(samples * sizeof(float), true), samples, samples);
    inputBuffer = CircularBuffer((float*)sdram_alloc(samples * sizeof(float), true), samples, samples);

    maxSamples = samples;

}

float AllPassFilter::applyOne(float in) {
    float nextRead = feedbackBuffer.readNext();
    
    feedbackBuffer.writeNext(-gain * in + inputBuffer.readNext() + nextRead * gain);
    inputBuffer.writeNext(in);

    return nextRead;
}


void AllPassFilter::apply(const float* in, float* out, size_t samples) {

    for (size_t i = 0; i < samples; i++) {

        out[i] += applyOne(in[i]);        

    }

}

void AllPassFilter::setParams(float gain, float depth) {
    this->gain = gain * 0.9f;
    inputBuffer.setWriteOffset(maxSamples * depth);
    feedbackBuffer.setWriteOffset(maxSamples * depth);
}

void AllPassFilter::setGain(float gain) {
    this->gain = gain;
}

void AllPassFilter::offsetSamples(int samples) {
    inputBuffer.setWriteOffset(samples);
    feedbackBuffer.setWriteOffset(samples);
}

// Looper

Looper::Looper(bool series) : Effect(series), buffer((float*)sdram_alloc(MAX_LOOP_SAMPLES)) {
    enabled = EffectParameter("Loop Playback", false);
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


// Delay

Delay::Delay(bool series) : Effect(series) {
    buffer = CircularBuffer((float*)sdram_alloc(DELAY_SIZE * sizeof(float), true), DELAY_SIZE, 0);
    enabled = EffectParameter("> Delay", false);
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
    enabled = EffectParameter("> Tuner", false);
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

    enabled = EffectParameter("> Reverb", false);

}

void Reverb::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return;
    

    for (size_t i = 0; i < REVERB_NUM_COMB_FILTERS; i++) {
        combFilters[i].apply(in, out, samples);
    }

    for (size_t j = 0; j < samples; j++) {

        float output = out[j];

        for (size_t i = 0; i < REVERB_NUM_AP_FILTERS; i++) {

            output = allPassFilters[i].applyOne(output); // series application

            // allPassFilters[i].apply(out, out, samples); // out -> out series application // IF THIS IS ADDING BACK TO OUTPUT THEN ITS NOT REALLY SERIES (See phaser)
        } 

        out[j] += output; // maybe make just assignment

    }

}

void Reverb::update() {

    for (int i = 0; i < REVERB_NUM_COMB_FILTERS; i++) {
        combFilters[i].setParams(depth.value, mix.value);
    }

    for (int i = 0; i < REVERB_NUM_AP_FILTERS; i++) {
        allPassFilters[i].setParams(gain.value, depth.value);
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

    enabled = EffectParameter("> Chorus", false);
    
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

    osc.SetFreq(8 * frequency.value + 0.1f); // maybe make this logarithmic
    delaySamples = maxDelaySamples * depth.value;

}


// BitCrusher
// lowk doesnt work rn for some reason
BitCrusher::BitCrusher(bool series) : Effect(series) {
    enabled = EffectParameter("> Bit Crusher", false);
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



float AllPassFilter2::process(float in) {
    float output = coefficient * in + inputSample - coefficient * outputSample;
    inputSample = in;
    outputSample = output;
    return output;
}



Phaser::Phaser(bool series) : Effect(series) {

    enabled = EffectParameter<bool>("> Phaser", false);

    for (int i = 0; i < PHASER_NUM_AP_FILTERS; i++) {
        allPassFilters[i] = AllPassFilter2(); //AllPassFilter((2 * BLOCK_SIZE) / (float)SAMPLE_RATE, 0.1f);
        // allPassFilters[i].offsetSamples(1);
    
    }

}

// lowk think this is a tremelo i think but idk how to make it a phaser or if thats even that different
void Phaser::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return;

    for (size_t i = 0; i < samples; i++) {

        float output = out[i] + heldSample * feedback.value * 0.99;

        oscPhase += 2 * PI_F * (4 * rate.value + 0.1f) / SAMPLE_RATE;
        if (oscPhase >= 2 * PI_F)
            oscPhase -= 2 * PI_F;

        // float stagePhaseOffset = PI_F / PHASER_NUM_AP_FILTERS;

        for (int j = 0; j < PHASER_NUM_AP_FILTERS; j++) {
            
            // float stageVal = (sinf(oscPhase + j * (PI_F / PHASER_NUM_AP_FILTERS)) * depth.value + 1) * 0.5f;
            // float fc = LFO_MIN_FREQ + stageVal * (LFO_MAX_FREQ - LFO_MAX_FREQ);

            // float cotanw = 1 / tanf(PI_F * fc / SAMPLE_RATE);
            // allPassFilters[j].coefficient = (1 - cotanw) / (1 + cotanw);
            // allPassFilters[j].setGain((tanVal - 1) / (tanVal + 1));

            float a = (sinf(oscPhase + j * (PI_F / (PHASER_NUM_AP_FILTERS * 8))) * depth.value + 1) * 0.5f;
            allPassFilters[j].coefficient = a;

            output = /* applyfilter( */allPassFilters[j].process(output)/* ) */; // actual series application
        }

        heldSample = output;


        // TODO: in general maybe i should do wet/dry mixing properly where its out[i] = out[i] * (1 - mix) + output * mix;
        out[i] += output;

    }   

}

Metronome::Metronome() : Effect(true) {
    enabled = EffectParameter<bool> ("> Metronome", false);
}

void Metronome::apply(const float* in, float* out, size_t samples) {

    if (!enabled.value) return;

    for (size_t i = 0; i < samples; i++) {

        // doing it this way is less efficient but more accurate by a little bit and i think that matters i guess
        waitCount++;

        if (waitCount >= 60 * SAMPLE_RATE / bpm) { // 60 -> seconds per minute
            shouldPlay = true;
            waitCount = 0;
            playbackSample = 0;
        }

        if (!shouldPlay) continue;

        out[i] += metronomeClick[playbackSample] * volume.value * 0.15f;
        
        downsampleCount++;

        if (downsampleCount >= 3){
            playbackSample++;
            downsampleCount = 0;
        }

        if (playbackSample >= METRONOME_CLICK_SIZE)
            shouldPlay = false;

    }

}
