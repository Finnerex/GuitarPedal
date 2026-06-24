#include <string.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "util.hpp"

using namespace daisy;

DaisySeed hw;

#define BLOCK_SIZE 16
#define SAMPLE_RATE 48000

// delay pedal stuff
#define MAX_DELAY_SECONDS 0.5f
#define DELAY_SIZE (int)(SAMPLE_RATE * MAX_DELAY_SECONDS)

float delayBuffer[DELAY_SIZE];
float delayPercent;

// reverb stuff (basically delay but different)




// looper pedal stuff
#define MAX_LOOP_SECONDS 60
#define MIN_LOOP_SAMPLES SAMPLE_RATE

float DSY_SDRAM_BSS loopBuffer[SAMPLE_RATE * MAX_LOOP_SECONDS];

int loopSamples = 0;
int currentLoopSample = 0;

bool recording;
bool looping; 


// dft stuff
#define DFT_NEW_WINDOW_SAMPLES 16384 //(int)(0.5f * SAMPLE_RATE) // (not right now) quarter second updates for now
#define DFT_OVERLAP_SAMPLES DFT_NEW_WINDOW_SAMPLES //(int)(0.7f * DFT_NEW_WINDOW_SAMPLES) // (not right now) half overlap (or is it a third)
#define DFT_WINDOW_SAMPLES (DFT_NEW_WINDOW_SAMPLES + DFT_OVERLAP_SAMPLES)

// double buffer: one to copy to and one to process
float DSY_SDRAM_BSS dftTimeBufferA[DFT_WINDOW_SAMPLES];
float DSY_SDRAM_BSS dftTimeBufferB[DFT_WINDOW_SAMPLES];
bool usingBufferA;
int currentDftWindowSamples;
std::complex<float> dftFrequencyBuffer[DFT_WINDOW_SAMPLES]; // maybe zero pad, idk. (f = k*fs/N)
bool newDftReady = false;

bool tunerEnabled = false;
bool looperEnabled = false;
bool delayEnabled = false;

static void Callback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{

    // passthrough copy
    memcpy(out[0], in[0], size * sizeof(float));

    // dft
    if (tunerEnabled)
    {
        if (usingBufferA)
            memcpy(&dftTimeBufferA[currentDftWindowSamples], in[0], size * sizeof(float));
        else
            memcpy(&dftTimeBufferB[currentDftWindowSamples], in[0], size * sizeof(float));

        currentDftWindowSamples += size;

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

    // looper
    if (looperEnabled)
    {
        if (recording) {
            
            if (loopSamples + size < SAMPLE_RATE * MAX_LOOP_SECONDS) {
                memcpy(&loopBuffer[loopSamples], in[0], size * sizeof(float));
                loopSamples += size;
            }

        } else if (looping && loopSamples >= MIN_LOOP_SAMPLES) {

            for (int i = 0; i < size; i++) {

                out[0][i] += loopBuffer[currentLoopSample + i];

            }

            currentLoopSample += size;
            currentLoopSample %= loopSamples;

        }
    }

    
    // delay
    if (delayEnabled) {
        static int writePos = 0;
        int readPos = (int)(writePos + DELAY_SIZE * delayPercent) % DELAY_SIZE;
        
        for (int i = 0; i < size; i++) {

            delayBuffer[(writePos + i) % DELAY_SIZE] = in[0][i];
            out[0][i] += delayBuffer[(readPos + i) % DELAY_SIZE];

        }

        writePos += size;
        writePos %= DELAY_SIZE;
    }   
}


void draw(void);
void step(void);

using Display = OledDisplay<SSD130xI2c128x64Driver>;
Display display;

Switch recordSwitch;
Switch playbackSwitch;
float maxFrequency;

Switch modeButton;
char mode = 0;


int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(BLOCK_SIZE); // each callback will process BLOCK_SIZE samples (probably wont overlap), 2 channel buffers will contain 2 * that many entries each
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    hw.StartAudio(Callback);

    AdcChannelConfig adcConfig;
    adcConfig.InitSingle(seed::A0);
    hw.adc.Init(&adcConfig, 1);
    hw.adc.Start();

    Display::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_address = 0x3C;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.speed = I2CHandle::Config::Speed::I2C_400KHZ;
    disp_cfg.driver_config.transport_config.i2c_config.mode = I2CHandle::Config::Mode::I2C_MASTER;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed::D11;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed::D12;
    
    display.Init(disp_cfg);

    recordSwitch.Init(seed::D0, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_NORMAL, Switch::PULL_NONE);
    playbackSwitch.Init(seed::D1, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_NORMAL, Switch::PULL_NONE);

    modeButton.Init(seed::D2, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_NORMAL, Switch::PULL_DOWN);

    while(true) {

        step();

        
        if (tunerEnabled) {
            draw();
            display.Update();
        }

    }
}

void step(void) { // could return a bool for stopping but idk if thats needed

    modeButton.Debounce();
    if (modeButton.RisingEdge()) {
        mode ++;
        mode %= 0b1000;
        for (int i = 0; i < mode; i++) {
            hw.SetLed(true);
            System::Delay(200);
            hw.SetLed(false);
            System::Delay(200);
        }
    }

    looperEnabled = mode & (1 << 0);
    delayEnabled = mode & (1 << 1);
    tunerEnabled = mode & (1 << 2);


    // looper

    if (looperEnabled) {
        recordSwitch.Debounce();
        playbackSwitch.Debounce();

        recording = recordSwitch.Pressed();
        looping = playbackSwitch.Pressed();

        if (recordSwitch.RisingEdge()) 
            loopSamples = 0; // new recording

        if (playbackSwitch.RisingEdge() || recordSwitch.FallingEdge())
            currentLoopSample = 0; // play from beginning
    }
    
    // dft / tuner
    if (newDftReady && tunerEnabled) { // hopefully this happens between buffer swaps - this isnt completely safe because the buffer could swap twice during this run i think and mess things up

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


    // delay
    if (delayEnabled) {
        float pct = hw.adc.GetFloat(0); // 0 - 1

        if (std::abs(delayPercent - pct) > 0.02f) { // ie step in 0.01 increments
            delayPercent = pct;
            hw.SetLed(true);
            System::Delay(200);
            hw.SetLed(false);
        }
    
        System::Delay(2);
    }

}


void draw(void) {


    // if (tunerEnabled)
    // {
        // im gonna do the frequency to note here because its easier

        int octave;
        const char* note;
        float error = frequencyToNote(maxFrequency, &note, &octave);

        char frequencyString[8] = "freq--";
        snprintf(frequencyString, 8, "%d", (int)maxFrequency);
        
        char octaveString[2] = "";
        if (octave >= 0 && octave <= 9)
            snprintf(octaveString, 2, "%d", octave);
        
        char errorString[4] = "";
        if (error > -1 && error < 1)
            snprintf(errorString, 4, "%d", (int)(error * 100));

        display.Fill(false);

        // tuner
        // bounding boxes
        display.DrawRect(3, 3, 82, 61, true, false);
        display.DrawRect(85, 3, 124, 61, true, false);

        // text
        display.WriteStringAligned(note, Font_16x26, Rectangle(85, 3, 39, 38), Alignment::centered, true);
        display.WriteStringAligned(octaveString, Font_11x18, Rectangle(85, 41, 39, 20), Alignment::centered, true);
        display.WriteStringAligned(errorString, Font_11x18, Rectangle(3, 41, 77, 20), Alignment::centered, true);
        display.WriteStringAligned(frequencyString, Font_11x18, Rectangle(3, 8, 77, 20), Alignment::centered, true);

        // dial (TODO)

    // }
    // display.SetCursor(2, 0);
    // display.WriteChar('0' + recording, Font_11x18, true);
    // display.WriteChar('0' + looping, Font_11x18, true);

    // display.SetCursor(2, 16);
    // display.WriteString(freqStr, Font_6x8, true);

}
