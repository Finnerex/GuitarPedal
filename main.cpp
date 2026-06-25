#include <string.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "util.hpp"
#include "effect.hpp"

using namespace daisy;

DaisySeed hw;

// delay pedal stuff
// maybe move these into the class
#define MAX_DELAY_SECONDS 0.5f
#define DELAY_SIZE (int)(SAMPLE_RATE * MAX_DELAY_SECONDS)
float delayBuffer[DELAY_SIZE];

// reverb stuff (basically delay but different)




// looper pedal stuff
#define MAX_LOOP_SECONDS 60
#define MIN_LOOP_SAMPLES SAMPLE_RATE

float DSY_SDRAM_BSS loopBuffer[SAMPLE_RATE * MAX_LOOP_SECONDS];


#define NUM_EFFECTS 3 // idk maybe the user will be able to add more in the interface
Effect* effects[NUM_EFFECTS];

using Display = OledDisplay<SSD130xI2c128x64Driver>;
Display display;

static void Callback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // passthrough copy
    memcpy(out[0], in[0], size * sizeof(float));

    // effects[2]->apply(in[0], out[0], size);

    for (int i = 0; i < NUM_EFFECTS; i++) {
        // if (effects[i] == nullptr) continue;

        if (effects[i]->series)
            effects[i]->apply(out[0], out[0], size);
        else
            effects[i]->apply(in[0], out[0], size);
    }
}


void draw(void);
void step(void);

float* maxFrequency;

#define NUM_VARIABLE_CONTROLS 1
VariableControl* potentiometers[NUM_VARIABLE_CONTROLS];

#define NUM_TOGGLE_CONTROLS 4
ToggleControl* buttons[NUM_TOGGLE_CONTROLS];

#define NUM_BOOL_PARAMETERS 4
#define NUM_FLOAT_PARAMETERS 1
EffectParameter<bool>* boolParams[NUM_BOOL_PARAMETERS];
EffectParameter<float>* floatParams[NUM_FLOAT_PARAMETERS]; 

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(BLOCK_SIZE); // each callback will process BLOCK_SIZE samples (probably wont overlap), 2 channel buffers will contain 2 * that many entries each
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);


    // display
    Display::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_address = 0x3C;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.speed = I2CHandle::Config::Speed::I2C_400KHZ;
    disp_cfg.driver_config.transport_config.i2c_config.mode = I2CHandle::Config::Mode::I2C_MASTER;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed::D11;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed::D12;
    
    display.Init(disp_cfg);


    // effects
    Looper* looper = new Looper(true, loopBuffer, SAMPLE_RATE * MAX_LOOP_SECONDS);
    boolParams[0] = &looper->enabled;
    boolParams[1] = &looper->recordingEnabled;

    Delay* delay = new Delay(true, delayBuffer, DELAY_SIZE, 0.5f * DELAY_SIZE);
    boolParams[2] = &delay->enabled;
    floatParams[0] = &delay->delayAmount;

    Tuner* tuner = new Tuner(false);
    boolParams[3] = &tuner->enabled;
    maxFrequency = &tuner->maxFrequency;
    
    effects[0] = tuner; // this kinda defeats some of the purpose
    effects[1] = delay;
    effects[2] = looper; 

    // pots
    AdcChannelConfig configs[NUM_VARIABLE_CONTROLS];
    for (int i = 0; i < NUM_VARIABLE_CONTROLS; i++) {
        potentiometers[i] = new VariableControl(&hw, &configs[i], hw.GetPin(15 + i));
        potentiometers[i]->parameter = floatParams[i];
    }

    hw.adc.Init(configs, NUM_VARIABLE_CONTROLS);
    hw.adc.Start();

    // buttons
    for (int i = 0; i < NUM_TOGGLE_CONTROLS; i++) {
        buttons[i] = new ToggleControl(hw.GetPin(i));
        buttons[i]->parameter = boolParams[i];
    }

    display.Fill(false);
    display.SetCursor(10, 10);
    display.WriteString("setup", Font_11x18, true);
    display.Update();

    // System::Delay(1000);

    hw.StartAudio(Callback);

    display.Fill(false);
    display.SetCursor(10, 10);
    display.WriteString("audio", Font_11x18, true);
    display.Update();

    // System::Delay(1000);

    while(true) {

        step();
        
        draw();
        display.Update();

        // static int l = 0;
        
        // display.SetCursor(10, 10);
        // display.WriteString("drew", Font_11x18, true);
        // display.WriteChar(48 + l++, Font_11x18, true);

        // display.SetCursor(10, 32);
        // display.WriteChar(48 + (int)potentiometers[0]->parameter->value, Font_11x18, true);
        // display.WriteChar('.', Font_11x18, true);
        // display.WriteChar(48 + (int)(potentiometers[0]->parameter->value * 10) % 10, Font_11x18, true);
        // display.WriteChar(48 + (int)(potentiometers[0]->parameter->value * 100) % 10, Font_11x18, true);

        // display.Update();

        // l %= 10;
    }
}

void step(void) { // could return a bool for stopping but idk if thats needed

    for (int i = 0; i < NUM_EFFECTS; i++) {
        effects[i]->update();
    }

    for (int i = 0; i < NUM_VARIABLE_CONTROLS; i++) {
        potentiometers[i]->update();
    }

    for (int i = 0; i < NUM_TOGGLE_CONTROLS; i++) {
        buttons[i]->update();
    }

    System::Delay(20);

}


void draw(void) {


    // im gonna do the frequency to note here because its easier

    
    int octave;
    const char* note;
    float error = frequencyToNote(*maxFrequency, &note, &octave);

    char frequencyString[8] = "freq--";
    snprintf(frequencyString, 8, "%d", (int)*maxFrequency);
    
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



}
