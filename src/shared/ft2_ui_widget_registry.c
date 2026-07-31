#include "shared/ft2_ui_schema.h"

const ft2_ui_widget_type_desc_t ft2_ui_widget_types[] =
{
    { FT2_UI_WIDGET_PUSHBUTTON,    "Button",   59, 16, false },
    { FT2_UI_WIDGET_RADIOBUTTON,   "Radio",    FT2_UI_RADIOBUTTON_W, FT2_UI_RADIOBUTTON_H, true },
    { FT2_UI_WIDGET_CHECKBOX,      "Checkbox", FT2_UI_CHECKBOX_W, FT2_UI_CHECKBOX_H, true },
    { FT2_UI_WIDGET_SCROLLBAR,     "Scroll",   75, 13, false },
    { FT2_UI_WIDGET_TEXTBOX,       "TextBox",  120, 16, false },
    { FT2_UI_WIDGET_FRAMEBOX,      "Frame",    120, 80, false },
    { FT2_UI_WIDGET_BITMAP,        "Bitmap",   64, 32, false },
    { FT2_UI_WIDGET_POPUP_LIST,    "Popup",    120, 80, false },
    { FT2_UI_WIDGET_WAVEFORM_VIEW, "Wave",     284, 72, false },
    { FT2_UI_WIDGET_TF_BUTTON,     "TF Btn",   48, 16, false },
    { FT2_UI_WIDGET_TF_TOGGLE_BUTTON, "TF Tgl", 40, 14, false },
    { FT2_UI_WIDGET_TF_LABEL,      "TF Label", 80, 12, false },
    { FT2_UI_WIDGET_TF_ROTARY_SLIDER, "TF Knob", 36, 36, false },
    { FT2_UI_WIDGET_TF_LINEAR_SLIDER, "TF Slider", 20, 52, false },
    { FT2_UI_WIDGET_TF_COMBO_BOX,  "TF Combo", 110, 20, false },
    { FT2_UI_WIDGET_TF_LEVEL_METER, "TF Meter", 40, 18, false },
    { FT2_UI_WIDGET_TF_PARAMETER_CONTROL, "TF Param", 40, 11, false },
    { FT2_UI_WIDGET_TF_ENVELOPE_DISPLAY, "TF Env", 284, 72, false },
    { FT2_UI_WIDGET_TF_GROUP_BOX,  "TF Group", 120, 80, false },
    { FT2_UI_WIDGET_MIXER_STRIP,   "Mix Strip", 25, 192, false },
    { FT2_UI_WIDGET_MIXER_GAIN,    "Mix Gain", 20, 80, false },
    { FT2_UI_WIDGET_MIXER_PAN,     "Mix Pan", 20, 7, false },
    { FT2_UI_WIDGET_MIXER_MUTE,    "Mix Mute", 23, 12, false },
    { FT2_UI_WIDGET_MIXER_SCOPE,   "Mix Scope", 20, 20, false },
    { FT2_UI_WIDGET_MIXER_MASTER,  "Mix Master", 40, 80, false },
    { FT2_UI_WIDGET_DSP_WINDOW,    "DSP Win", 192, 120, false },
    { FT2_UI_WIDGET_DSP_SLOT,      "DSP Slot", 180, 14, false },
    { FT2_UI_WIDGET_DSP_MENU,      "DSP Menu", 80, 16, false },
    { FT2_UI_WIDGET_DSP_PARAM,     "DSP Param", 100, 16, false },
    { FT2_UI_WIDGET_TF_ARP_STEP,   "TF Arp Step", 18, 96, false }
};

const uint16_t ft2_ui_widget_type_count =
    (uint16_t)(sizeof(ft2_ui_widget_types) / sizeof(ft2_ui_widget_types[0]));
