#ifndef GUI_H
#define GUI_H

#include "daisy_seed.h"
#include "effect.hpp"
#include "dev/oled_ssd130x.h"

using namespace daisy;
using Display = OledDisplay<SSD130xI2c128x64Driver>;

/*

UI Design

Start screen (window):
 - back button turns screen off (select or back turns back on)
 - scroll selection containing
    - Utilities
    - Effects
    - Controls
 - something on the right idk what it will be

Utilities:
scroll section with
 - Tuner
    - selecting takes you to the tuner screen i already made and enables the tuner
    - maybe also have this be an effect that can be assigned a button?
 - Metronome / Click track
    - takes you to a screen where you can set metronome frequency and volume and all that

Effects: (maybe unnnecccecetryyh (not needed))
 - scroll section with all of the effects
 - selecting an effect will take you to a screen where [[idk]]
 - right side could show parameters and controls they are mapped to (this wont fit)
 - maybe application order can be changed here

Controls:
 - scroll with all of the controls: buttons 1-4 and pots 1-4
 - selecting a control will give a menu of parameters or effects to select
    - if effects, selecting an effect will bring a further menu with its parameters

*/




class Window {
public:
// back button should always take to previous, if null, turn screen off or something
    Window* previous;

    virtual void update(bool encoderPress, int encoderMove, Window** currentWindow) = 0;
    virtual void draw(Display* d) = 0; // TODO: maybe display dirtying system where only redrawn on dirty

};



class ScrollElement {
public:
    Window* subWindow;
    const char* name;   

    ScrollElement() : subWindow(nullptr), name(nullptr) {}
    ScrollElement(Window* subWindow, const char* name) : subWindow(subWindow), name(name) {}

    virtual void OnSelect(Window** currentWindow) {};

};

template <typename T>
class ParameterScrollElement : public ScrollElement {
public:

    EffectParameter<T>* parameter;

    ParameterScrollElement() : ScrollElement(), parameter(nullptr) {}
    ParameterScrollElement(EffectParameter<T>* parameter/* , const char* name */) : ScrollElement(nullptr, parameter->name), parameter(parameter) {}

    void OnSelect(Window** currentWindow) override;

};

class ToggleControlScrollElement : public ScrollElement {
    
    ToggleControl* control;

public:

    char nameBuf[9]; // this is dumb

    ToggleControlScrollElement(ToggleControl* button) : ScrollElement(nullptr, nameBuf), control(button) {}
    void OnSelect(Window** currentWindow) override;

};

class VariableControlScrollElement: public ScrollElement {

    VariableControl* control;

public:

    char nameBuf[7];

    VariableControlScrollElement(VariableControl* pot) : ScrollElement(nullptr, nameBuf), control(pot) {}
    void OnSelect(Window** currentWindow) override;

};


class ScrollWindow : public Window {

    int numElements;
    int currentElement;
    ScrollElement** elements;

public:
    ScrollWindow(int numElements, ScrollElement** elements);
    ScrollWindow(int numElements, ScrollElement* realElements, ScrollElement** elements);
    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);

};

template <typename T>
class ParameterWindow : public Window {

    // different update & draw implementation depending on parameter type
    // int -> scroll changes number by 1/-1
    // float -> scroll changes number by 0.01 or something, maybe per decimal place idk, maybe have a scaler for that
    // bool -> select toggles

public:
    EffectParameter<T>* parameter;

    ParameterWindow() : parameter(nullptr) {}
    ParameterWindow(EffectParameter<T>* parameter) : parameter(parameter) {};

    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);

};


class SaveWindow : public Window {
    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);
};

class ResetWindow : public Window {
    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);
};

class TunerWindow : public Window {

public:
    Effect* tuner;

    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);
};

class InfoWindow : public Window {
    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);
};

class MetronomeWindow : public Window {

public:
    Metronome* metronome;

    void update(bool encoderPress, int encoderMove, Window** currentWindow);
    void draw(Display* d);
};

// maybe button/ pot assignment window
// waveform, spectrum, and control debug windows

extern EffectParameter<float> ledRed;
extern EffectParameter<float> ledGreen;
extern EffectParameter<float> ledBlue;

extern ScrollElement* varControlOptions[NUM_VARIABLE_CONTROLS]; 
extern ScrollElement* toggleControlOptions[NUM_TOGGLE_CONTROLS];
extern ScrollElement* parameterOptions[NUM_BOOL_PARAMETERS + NUM_FLOAT_PARAMETERS];
extern ScrollWindow mainWindow;

extern TunerWindow tunerWindow;
extern float* maxFrequency;

extern MetronomeWindow metronomeWindow;


#endif
