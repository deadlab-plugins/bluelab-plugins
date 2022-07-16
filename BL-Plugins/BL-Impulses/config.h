/* Copyright (C) 2022 Nicolas Dittlo <deadlab.plugins@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this software; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#define PLUG_NAME "BL-Impulses"
#define PLUG_MFR "BlueLab"
#define PLUG_VERSION_HEX 0x00060204
#define PLUG_VERSION_STR "6.2.4"
// http://service.steinberg.de/databases/plugin.nsf/plugIn?openForm
#define PLUG_UNIQUE_ID 'jesv'
#define PLUG_MFR_ID 'BlLa'
#define PLUG_URL_STR "http://www.bluelab-plugs.com"
#define PLUG_EMAIL_STR "contact@bluelab-plugs.com"
#define PLUG_COPYRIGHT_STR "Copyright 2021 BlueLab | Audio Plugins"
#define PLUG_CLASS_NAME Impulses

#define BUNDLE_NAME "BL-Impulses"
#define BUNDLE_MFR "BlueLab"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "BL-Impulses"

#define PLUG_CHANNEL_IO "1-1 1-2 2-2"

//#if USE_FAST_RT_CONVOLVER
#define PLUG_LATENCY 0
//#else

// Latency
// With 0, this is OK !
//#define PLUG_LATENCY 0 //2048

// Need latency, with the same size as convolver buffer size
// Otherwise if host buffer size is != 1024, there will
// be a time shift of the signal
//
// NOTE: with host buffer size > PLUG_LATENCY, latency will be unavoidable
//#define PLUG_LATENCY 1024
//#endif

#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 664
#define PLUG_HEIGHT 490
// Origin IPlug2: 60:
// Origin Rebalance: 25
// For Windows + VSynch
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY Impulses_Entry
#define AUV2_ENTRY_STR "Impulses_Entry"
#define AUV2_FACTORY Impulses_Factory
#define AUV2_VIEW_CLASS Impulses_View
#define AUV2_VIEW_CLASS_STR "Impulses_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "BlLa"
#define AAX_PLUG_NAME_STR "Impulses\nImpulses"
#define AAX_PLUG_CATEGORY_STR "SoundField"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx|Spatial"

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

#define MANUAL_FN "BL-Impulses_manual-EN.pdf"
// For WIN32
#define MANUAL_FULLPATH_FN "manual\BL-Impulses_manual-EN.pdf"

#define DUMMY_RESOURCE_FN "dummy.txt"

// Image resource locations for this plug.
#define BACKGROUND_FN "background.png"
#define PLUGNAME_FN "plugname.png"
#define HELP_BUTTON_FN "help-button.png"
#define TEXTFIELD_FN "textfield.png"
#define RADIOBUTTON_FN "radiobutton.png"

#define KNOB_SMALL_FN "knob-small.svg"

#define GRAPH_FN "graph.png"

// GUI resize
#define BUTTON_OPEN_FN "button-open.png"
#define BUTTON_SAVE_FN "button-save.png"
#define BUTTON_RESET_FN "button-reset.png"
