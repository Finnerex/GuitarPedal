#include "gui.hpp"


ScrollWindow::ScrollWindow(int numElements, ScrollElement** elements) : numElements(numElements), elements(elements) {}

ScrollWindow::ScrollWindow(int numElements, ScrollElement* realElements, ScrollElement** elements) : numElements(numElements), elements(elements) {
    for (int i = 0; i < numElements; i++) {
        elements[i] = &realElements[i];
    }
}

void ScrollWindow::update(bool encoderPress, int encoderMove, Window** currentWindow) {

    if (encoderPress) {
        elements[currentElement]->OnSelect(currentWindow);

        Window* next = elements[currentElement]->subWindow; // maybe change to have window scroll element which does this in its onselect
        next->previous = *currentWindow; // happens for things i dont want it to (control scroll elements), making back go to bad screens, TODO: fix
        *currentWindow = next;
        
        mainWindow.previous = nullptr; // what a solution for ^^
        return;
    }

    currentElement = std::clamp(currentElement + encoderMove, 0, numElements - 1);

}


#define ELEMENTS_PER_WINDOW 5
void ScrollWindow::draw(Display* d) {
    
    d->DrawLine(1, 2, 1, 11, true); // left side line to show selection

    for (int i = currentElement; i < currentElement + ELEMENTS_PER_WINDOW && i < numElements; i++) {
        int y = (i - currentElement) * 13; // top left of outside box

        // way too many arcs - maybe make it just a rectangle
        d->DrawArc(3+7, y+6, 7, 180, 90, true); // tl quarter
        d->DrawArc(3+7, y+7, 7, 90, 90, true); // bl quarter
        d->DrawArc(d->Width() - (3+7), y+6, 7, 270, 90, true); // tr quarter
        d->DrawArc(d->Width() - (3+7), y+7, 7, 0, 90, true); // br quarter

        d->DrawLine(3+7, y, d->Width() - (3+7), y, true);

        d->SetCursor(3+8, y + 2);
        d->WriteString(elements[i]->name, Font_7x10, true);

        if (i == numElements - 1 && y + 13 < 64) {
            d->DrawLine(3+7, y + 13, d->Width() - (3+7), y + 13, true);
        }

    }


}

EffectParameter<float>* selectedFloatParam;
ScrollElement* varControlOptions[NUM_VARIABLE_CONTROLS]; 
ScrollWindow variableControlsWindow = ScrollWindow(NUM_VARIABLE_CONTROLS, varControlOptions);

template<> void ParameterWindow<float>::update(bool encoderPress, int encoderMove, Window **currentWindow) {

    if (encoderPress) {
        selectedFloatParam = parameter;

        variableControlsWindow.previous = *currentWindow;
        *currentWindow = &variableControlsWindow;
        
        return;
    }

    parameter->value = std::clamp(parameter->value + encoderMove * 0.01f, 0.f, 1.f);
}

EffectParameter<bool>* selectedBoolParam;
ScrollElement* toggleControlOptions[NUM_TOGGLE_CONTROLS];
ScrollWindow toggleControlsWindow = ScrollWindow(NUM_TOGGLE_CONTROLS, toggleControlOptions);

template<> void ParameterWindow<bool>::update(bool encoderPress, int encoderMove, Window **currentWindow) {

    if (encoderPress) {
        selectedBoolParam = parameter;

        toggleControlsWindow.previous = *currentWindow;
        *currentWindow = &toggleControlsWindow;

        return;
    }

    if (encoderMove < 0)
        parameter->value = false;
    else if (encoderMove > 0)
        parameter->value = true;
}

char drawPctBuf[5];

template<> void ParameterWindow<float>::draw(Display* d) {

    d->WriteStringAligned(parameter->name, Font_7x10, Rectangle(2, 2, 124, 20), Alignment::centered, true);
    d->DrawLine(0, 20, 127, 20, true);
    
    snprintf(drawPctBuf, 5, "%d%%", (int)(parameter->value * 100));

    d->WriteStringAligned(drawPctBuf, Font_16x26, Rectangle(0, 21, 127, 42), Alignment::centered, true);

}

template<> void ParameterWindow<bool>::draw(Display* d) {
    
    d->WriteStringAligned(parameter->name, Font_7x10, Rectangle(2, 2, 124, 20), Alignment::centered, true);
    d->DrawLine(0, 20, 127, 20, true);

    d->WriteStringAligned(parameter->value ? "ON" : "OFF", Font_16x26, Rectangle(0, 21, 127, 42), Alignment::centered, true);

}

ParameterWindow<float> floatParamWindow;

template<> void ParameterScrollElement<float>::OnSelect(Window** currentWindow) {
    subWindow = &floatParamWindow;
    floatParamWindow.parameter = parameter;
}

ParameterWindow<bool> boolParamWindow;

template<> void ParameterScrollElement<bool>::OnSelect(Window** currentWindow) {
    subWindow = &boolParamWindow;
    boolParamWindow.parameter = parameter;
}

void ToggleControlScrollElement::OnSelect(Window** currentWindow) {

    ToggleControl* oldControl = static_cast<ToggleControl*>(selectedBoolParam->control); 
    if (oldControl != nullptr) {
        oldControl->parameter = nullptr;
    }

    control->parameter = selectedBoolParam;
    selectedBoolParam->control = control;
}

void VariableControlScrollElement::OnSelect(Window** currentWindow) {

    VariableControl* oldControl = static_cast<VariableControl*>(selectedFloatParam->control); // have to set previous control's parameter to null
    if (oldControl != nullptr) {
        oldControl->parameter = nullptr;
    }
     
    control->parameter = selectedFloatParam; 
    selectedFloatParam->control = control;
}

void SaveWindow::draw(Display* d) {

    d->DrawRect(15, 10, 127-15, 63-10, true);
    d->WriteStringAligned("Save?", Font_16x26, Rectangle(15, 10, 127-30, 63-20), Alignment::centered, true);

}

void SaveWindow::update(bool encoderPress, int encoderMove, Window** currentWindow) {

    if (!encoderPress) return;

    PersistentSettings& localSettings = persistentData.GetSettings();

    for (int i = 0; i < NUM_BOOL_PARAMETERS; i++) {
        localSettings.boolParams[i] = boolParams[i]->save();
    }

    for (int i = 0; i < NUM_FLOAT_PARAMETERS; i++) {
        localSettings.floatParams[i] = floatParams[i]->save();
    }

    localSettings.ledR = ledRed.value;
    localSettings.ledG = ledGreen.value;
    localSettings.ledB = ledBlue.value;

    localSettings.metronomeBpm = metronomeWindow.metronome->bpm;

    persistentData.Save();

    *currentWindow = &mainWindow; 

}

void ResetWindow::draw(Display* d) {

    d->DrawRect(15, 15, 127-15, 63-15, true);
    d->WriteStringAligned("Reset?", Font_11x18, Rectangle(15, 15, 127-30, 63-30), Alignment::centered, true);

}

void ResetWindow::update(bool encoderPress, int encoderMove, Window** currentWindow) {

    if (!encoderPress) return;
        
    for (int i = 0; i < NUM_TOGGLE_CONTROLS; i++) {
        buttons[i]->parameter = nullptr;
    }

    for (int i = 0; i < NUM_VARIABLE_CONTROLS; i++) {
        potentiometers[i]->parameter = nullptr;
    }

    for (int i = 0; i < NUM_BOOL_PARAMETERS; i++) {
        boolParams[i]->value = boolParams[i]->initialValue;
        boolParams[i]->control = nullptr;
    }

    for (int i = 0; i < NUM_FLOAT_PARAMETERS; i++) {
        floatParams[i]->value = floatParams[i]->initialValue;
        floatParams[i]->control = nullptr;
    }

    metronomeWindow.metronome->bpm = 100;
    ledRed.value = 0.5f;
    ledGreen.value = 0.5f;
    ledBlue.value = 0.5f;

    *currentWindow = &mainWindow; 

}


float* maxFrequency;

void TunerWindow::draw(Display* d) {
    int octave;
    const char* note;
    float error = frequencyToNote(*maxFrequency, &note, &octave);

    char frequencyString[8] = "";
    snprintf(frequencyString, 8, "%d", (int)*maxFrequency);
    
    char octaveString[2] = "";
    if (octave >= 0 && octave <= 9)
        snprintf(octaveString, 2, "%d", octave);
    
    char errorString[4] = "";
    if (error > -1 && error < 1)
        snprintf(errorString, 4, "%d", (int)(error * 100));

    d->Fill(false);

    // tuner
    // bounding boxes
    d->DrawRect(3, 3, 82, 61, true, false);
    d->DrawRect(85, 3, 124, 61, true, false);

    // text
    d->WriteStringAligned(note, Font_16x26, Rectangle(85, 3, 39, 38), Alignment::centered, true);
    d->WriteStringAligned(octaveString, Font_11x18, Rectangle(85, 41, 39, 20), Alignment::centered, true);
    d->WriteStringAligned(errorString, Font_11x18, Rectangle(3, 41, 77, 20), Alignment::centered, true);
    d->WriteStringAligned(frequencyString, Font_11x18, Rectangle(3, 8, 77, 20), Alignment::centered, true);

    // dial (TODO)
}

void TunerWindow::update(bool encoderPress, int encoderMove, Window** currentWindow) {

    if (encoderPress)
        tuner->enabled.value = !tuner->enabled.value;

}

char numStringBuf[4];
void InfoWindow::draw(Display* d) {

    for (int i = 0; i < NUM_VARIABLE_CONTROLS; i++) {

        int y = i * 8;

        d->SetCursor(0, y);
        d->WriteChar('0' + potentiometers[i]->id, Font_6x8, true);

        d->SetCursor(10, y);
        d->WriteString(potentiometers[i]->parameter != nullptr ? potentiometers[i]->parameter->name : "Unassigned", Font_6x8, true);

        d->SetCursor(110, y);
        if (potentiometers[i]->parameter != nullptr) {
            snprintf(numStringBuf, 4, "%d", (int)(100 * potentiometers[i]->parameter->value));
            d->WriteString(numStringBuf, Font_6x8, true);
        } else {
            d->WriteString("---", Font_6x8, true);
        }

    }

    for (int i = 0; i < NUM_TOGGLE_CONTROLS; i++) {

        int y = i * 8 + 8 * 4;

        d->SetCursor(0, y);
        d->WriteChar('0' + buttons[i]->id, Font_6x8, true);

        d->SetCursor(10, y);
        d->WriteString(buttons[i]->parameter != nullptr ? buttons[i]->parameter->name : "Unassigned", Font_6x8, true);

        d->SetCursor(110, y);
        if (buttons[i]->parameter != nullptr) {
            d->WriteString(buttons[i]->parameter->value ? "On" : "Off", Font_6x8, true);
        } else {
            d->WriteString("---", Font_6x8, true);
        }

    }

}

void InfoWindow::update(bool encoderPress, int encoderMove, Window** currentWindow) {
    // umm idk nothing needs to happen
}

char drawBpmBuf[8];
void MetronomeWindow::draw(Display* d) {

    d->WriteStringAligned("Metronome", Font_7x10, Rectangle(2, 2, 124, 20), Alignment::centered, true);
    d->DrawLine(0, 20, 127, 20, true);
    
    snprintf(drawBpmBuf, 8, "%d bpm", metronome->bpm);

    d->WriteStringAligned(drawBpmBuf, Font_11x18, Rectangle(0, 21, 127, 21), Alignment::centered, true);

    d->WriteStringAligned(metronome->enabled.value ? "ON" : "OFF", Font_11x18, Rectangle(0, 42, 127, 21), Alignment::centered, true);

}


void MetronomeWindow::update(bool encoderPress, int encoderMove, Window** currentWindow) {

    if (encoderPress)
        metronome->enabled.value = !metronome->enabled.value;

    metronome->bpm = std::clamp(metronome->bpm + encoderMove, 1, 500);

}


EffectParameter<float> ledRed = EffectParameter<float>("Red", 0.5f);
EffectParameter<float> ledGreen = EffectParameter<float>("Green", 0.5f);
EffectParameter<float> ledBlue = EffectParameter<float>("Blue", 0.5f);

ParameterScrollElement er = ParameterScrollElement(&ledRed), eg =  ParameterScrollElement(&ledGreen), eb = ParameterScrollElement(&ledBlue);
ScrollElement* ledOptions[3] = { &er, &eg, &eb };
ScrollWindow ledWindow = ScrollWindow(3, ledOptions);

ScrollElement* parameterOptions[NUM_FLOAT_PARAMETERS + NUM_BOOL_PARAMETERS];

ScrollWindow parametersWindow = ScrollWindow(NUM_BOOL_PARAMETERS + NUM_FLOAT_PARAMETERS, parameterOptions);

SaveWindow saveWindow;
ResetWindow resetWindow;
TunerWindow tunerWindow;
InfoWindow infoWindow;
MetronomeWindow metronomeWindow;

ScrollElement mainOptions[7] = { {&parametersWindow, "Parameters"}, {&saveWindow, "Save Settings"}, {&infoWindow, "Info"}, {&metronomeWindow, "Metronome"}, {&tunerWindow, "Tuner"}, {&resetWindow, "Reset"}, {&ledWindow, "RGB LED"} };
ScrollElement* a[7];
ScrollWindow mainWindow = ScrollWindow(7, mainOptions, a);

