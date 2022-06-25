#define PLUG_NAME "BL-Rebalance"
#define PLUG_MFR "BlueLab"
#define PLUG_VERSION_HEX 0x00060404
#define PLUG_VERSION_STR "6.4.4"
#define PLUG_UNIQUE_ID 'tjjm'
#define PLUG_MFR_ID 'BlLa'
#define PLUG_URL_STR "http://www.bluelab-plugs.com"
#define PLUG_EMAIL_STR "contact@bluelab-plugs.com"
#define PLUG_COPYRIGHT_STR "Copyright 2021 BlueLab | Audio Plugins"
#define PLUG_CLASS_NAME Rebalance

#define BUNDLE_NAME "BL-Rebalance"
#define BUNDLE_MFR "BlueLab"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "BL-Rebalance"

#define PLUG_CHANNEL_IO "1-1 2-2"

#define PLUG_LATENCY 2048
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 558
#define PLUG_HEIGHT 560
// Origin IPlug2: 60:
// Origin Rebalance: 25
// For Windows + VSynch
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY Rebalance_Entry
#define AUV2_ENTRY_STR "Rebalance_Entry"
#define AUV2_FACTORY Rebalance_Factory
#define AUV2_VIEW_CLASS Rebalance_View
#define AUV2_VIEW_CLASS_STR "Rebalance_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "BlLa"
#define AAX_PLUG_NAME_STR "Rebalance\nRebalance"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx|Tools"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

// Necessary!
#define ROBOTO_FN "Roboto-Regular.ttf"

#define FONT_REGULAR_FN "font-regular.ttf"
#define FONT_LIGHT_FN "font-light.ttf"
#define FONT_BOLD_FN "font-bold.ttf"

#define FONT_OPENSANS_EXTRA_BOLD_FN "OpenSans-ExtraBold.ttf"
#define FONT_ROBOTO_BOLD_FN "Roboto-Bold.ttf"

#define MANUAL_FN "BL-Rebalance_manual-EN.pdf"
// For WIN32
#define MANUAL_FULLPATH_FN "manual\BL-Rebalance_manual-EN.pdf"

#define DUMMY_RESOURCE_FN "dummy.txt"

// Image resource locations for this plug.
#define BACKGROUND_FN "background.png"
#define PLUGNAME_FN "plugname.png"
#define HELP_BUTTON_FN "help-button.png"
#define TEXTFIELD_FN "textfield.png"

#define KNOB_FN "knob.svg"
#define KNOB_SMALL_FN "knob-small.svg"

#define GRAPH_FN "graph.png"

// Specific
#define SOLO_TOGGLE_BUTTON_FN "button-solo.png"
#define MUTE_TOGGLE_BUTTON_FN "button-mute.png"


// Models for WIN32
#define MODEL0_FN "rebalance0.cfg"
// With Win32 configuration (not x64, we also need the full path)
#define MODEL0_FULLPATH_FN "models/rebalance0.cfg"

#define WEIGHTS0_FN "rebalance0.weights"
#define WEIGHTS0_FULLPATH_FN "models/rebalance0.weights"

#define MODEL1_FN "rebalance1.cfg"
#define MODEL1_FULLPATH_FN "models/rebalance1.cfg"

#define WEIGHTS1_FN "rebalance1.weights"
#define WEIGHTS1_FULLPATH_FN "models/rebalance1.weights"
