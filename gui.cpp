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

    persistentData.Save();

    *currentWindow = &mainWindow; 

}


ScrollElement* parameterOptions[NUM_FLOAT_PARAMETERS + NUM_BOOL_PARAMETERS];

ScrollWindow parametersWindow = ScrollWindow(NUM_BOOL_PARAMETERS + NUM_FLOAT_PARAMETERS, parameterOptions);

SaveWindow saveWindow;

ScrollElement mainOptions[6] = { {&parametersWindow, "Parameters"}, {&saveWindow, "Save Settings"}, {nullptr, "Metronome"}, {nullptr, "Tuner"}, {nullptr, "RGB LED"}, {nullptr, "Debug"} };
ScrollElement* a[6];
ScrollWindow mainWindow = ScrollWindow(6, mainOptions, a);

