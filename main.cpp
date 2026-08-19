#include <string.h>
#include "daisy_seed.h"
#include "util.hpp"
#include "effect.hpp"
#include "gui.hpp"

using namespace daisy;


#define NUM_EFFECTS 8 // idk maybe the user (I) will be able to add more in the interface
Effect* effects[NUM_EFFECTS];

static void Callback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // passthrough copy
    memcpy(out[0], in[0], size * sizeof(float));

    for (int i = 0; i < NUM_EFFECTS; i++) {
        // if (effects[i] == nullptr) continue;

        if (effects[i]->series)
            effects[i]->apply(out[0], out[0], size);
        else
            effects[i]->apply(in[0], out[0], size);
    }

}

#define CONTOLS_UPDATE_RATE 1000

RgbLed rgbLed;
Encoder encoder;
Switch backButton;

volatile int encoderMove;
volatile bool encoderPress;
volatile bool backPress;
volatile bool controlUpdateReady;

typedef struct {
    volatile int encoderMove;
    volatile bool encoderPress;
    volatile bool backPress;
} ControlData;

void ControlsTimerCallback(void* data) {
    
    // ControlData* cd = (ControlData*) data; // local doesnt seem to work lmao

    rgbLed.Update();

    backButton.Debounce();
    ((ControlData*)data)->backPress |= backButton.RisingEdge();

    encoder.Debounce();
    ((ControlData*)data)->encoderPress |= encoder.RisingEdge();

    ((ControlData*)data)->encoderMove += encoder.Increment();

    if (!controlUpdateReady) {
        encoderPress = ((ControlData*)data)->encoderPress;
        ((ControlData*)data)->encoderPress = false;

        backPress = ((ControlData*)data)->backPress;
        ((ControlData*)data)->backPress = false;

        encoderMove = ((ControlData*)data)->encoderMove;
        ((ControlData*)data)->encoderMove = 0;

        controlUpdateReady = true;
    }

}

void draw(void);
void step(void);

Display display;

Window* currentWindow;
Window* startWindow;

TimerHandle controlsTimerHandle;

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(BLOCK_SIZE);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // display
    Display::Config dispCfg;
    dispCfg.driver_config.transport_config.i2c_address = 0x3C;
    dispCfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    dispCfg.driver_config.transport_config.i2c_config.speed = I2CHandle::Config::Speed::I2C_400KHZ;
    dispCfg.driver_config.transport_config.i2c_config.mode = I2CHandle::Config::Mode::I2C_MASTER;
    dispCfg.driver_config.transport_config.i2c_config.pin_config.scl = seed::D11;
    dispCfg.driver_config.transport_config.i2c_config.pin_config.sda = seed::D12;
    
    display.Init(dispCfg);

    currentWindow = &mainWindow;
    startWindow = &mainWindow;

    int bp = 0, fp = 0;

    persistentData.Init({});

    // effects
    Looper* looper = new Looper(true);
    boolParams[bp++] = &looper->enabled;
    boolParams[bp++] = &looper->recordingEnabled;

    Delay* delay = new Delay(true);
    boolParams[bp++] = &delay->enabled;
    floatParams[fp++] = &delay->delayAmount;

    Tuner* tuner = new Tuner(false);
    boolParams[bp++] = &tuner->enabled;
    maxFrequency = &tuner->maxFrequency;
    tunerWindow.tuner = tuner;

    Reverb* reverb = new Reverb(false);
    boolParams[bp++] = &reverb->enabled;
    floatParams[fp++] = &reverb->depth;
    floatParams[fp++] = &reverb->mix;
    floatParams[fp++] = &reverb->gain;

    Chorus* chorus = new Chorus(true, 0.01f);
    boolParams[bp++] = &chorus->enabled;
    floatParams[fp++] = &chorus->frequency; 
    floatParams[fp++] = &chorus->depth;
    floatParams[fp++] = &chorus->mix;

    BitCrusher* bitCrusher = new BitCrusher(true);
    boolParams[bp++] = &bitCrusher->enabled;
    floatParams[fp++] = &bitCrusher->amount;

    Phaser* phaser = new Phaser(true);
    boolParams[bp++] = &phaser->enabled;
    floatParams[fp++] = &phaser->rate;
    floatParams[fp++] = &phaser->depth;
    floatParams[fp++] = &phaser->feedback;

    Metronome* metronome = new Metronome();
    boolParams[bp++] = &metronome->enabled;
    floatParams[fp++] = &metronome->volume;
    metronomeWindow.metronome = metronome;
    
    effects[0] = tuner;
    effects[1] = reverb;
    effects[2] = bitCrusher;
    effects[3] = delay;
    effects[4] = chorus;
    effects[5] = phaser;
    effects[6] = looper;
    effects[7] = metronome;

    char controlNameBuf[9];
    // pots
    AdcChannelConfig configs[NUM_VARIABLE_CONTROLS];
    for (int i = 0; i < NUM_VARIABLE_CONTROLS; i++) {
        potentiometers[i] = new VariableControl(&hw, &configs[i], hw.GetPin(15 + i), i);
        potentiometers[i]->parameter = nullptr;

        snprintf(controlNameBuf, 7, "Knob %d", i);

        VariableControlScrollElement* el = new VariableControlScrollElement(potentiometers[i]);
        el->subWindow = startWindow;
        strcpy(el->nameBuf, controlNameBuf);
        varControlOptions[i] = el;
    }

    hw.adc.Init(configs, NUM_VARIABLE_CONTROLS);
    hw.adc.Start();

    // buttons
    for (int i = 0; i < NUM_TOGGLE_CONTROLS; i++) {
        buttons[i] = new ToggleControl(hw.GetPin(i + 22), i);
        buttons[i]->parameter = nullptr;

        snprintf(controlNameBuf, 9, "Button %d", i);

        ToggleControlScrollElement* el = new ToggleControlScrollElement(buttons[i]);
        el->subWindow = startWindow;
        strcpy(el->nameBuf, controlNameBuf);
        toggleControlOptions[i] = el;
        
    }

    PersistentSettings& localSettings = persistentData.GetSettings();

    for (int i = 0; i < NUM_FLOAT_PARAMETERS; i++) {
        parameterOptions[i] = new ParameterScrollElement(floatParams[i]);
        floatParams[i]->control = nullptr;
        floatParams[i]->load(localSettings.floatParams[i]);
    }

    for (int i = 0; i < NUM_BOOL_PARAMETERS; i++) {
        parameterOptions[i + NUM_FLOAT_PARAMETERS] = new ParameterScrollElement(boolParams[i]);
        boolParams[i]->control = nullptr;
        boolParams[i]->load(localSettings.boolParams[i]);
    }

    ledRed.value = localSettings.ledR;
    ledGreen.value = localSettings.ledG;
    ledBlue.value = localSettings.ledB;

    metronome->bpm = localSettings.metronomeBpm;


    backButton.Init(seed::D14, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED, Switch::PULL_UP);
    encoder.Init(seed::D4, seed::D3, seed::D5);

    rgbLed.Init(seed::D0, seed::D1, seed::D2, false);
    

    TimerHandle::Config timerCfg;
    timerCfg.periph = TimerHandle::Config::Peripheral::TIM_5;
    timerCfg.dir = TimerHandle::Config::CounterDir::UP;
    timerCfg.period = System::GetPClk2Freq() * 2 / CONTOLS_UPDATE_RATE;
    timerCfg.enable_irq = true;

    volatile ControlData cd = { 0 };

    controlsTimerHandle.Init(timerCfg);
    controlsTimerHandle.SetCallback(ControlsTimerCallback, (void*)&cd);
    controlsTimerHandle.Start();

    hw.StartAudio(Callback);


    while(true) {

        step();
        draw();

    }
}

void step(void) { // could return a bool for stopping but idk if thats needed

    for (int i = 0; i < NUM_EFFECTS; i++) {
        effects[i]->update();
    }

    // TODO: maybe merge pots and buttons because i have polymorphism now
    
    for (int i = 0; i < NUM_VARIABLE_CONTROLS; i++) {
        potentiometers[i]->update();
    }

    for (int i = 0; i < NUM_TOGGLE_CONTROLS; i++) {
        buttons[i]->update();
    }


    if (currentWindow != nullptr) {
        currentWindow->update(encoderPress, encoderMove, &currentWindow);
        if (backPress) currentWindow = currentWindow->previous;

    } else if (backPress || encoderPress) {
        currentWindow = startWindow;
    }

    // probably isnt needed
    encoderPress = false;
    backPress = false;
    encoderMove = 0;

    rgbLed.Set(ledRed.value, ledGreen.value, ledBlue.value);

    controlUpdateReady = false;
    
}


void draw(void) {

    display.Fill(false);

    if (currentWindow != nullptr) {
        currentWindow->draw(&display);
    }

    display.Update();

}
