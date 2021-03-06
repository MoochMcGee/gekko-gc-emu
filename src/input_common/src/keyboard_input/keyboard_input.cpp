/*!
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    keyboard_input.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-03-03
 * @brief   Implementation of a keyboard GC controller interface
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include <SDL.h>

#include "common.h"
#include "config.h"

#include "input_common.h"
#include "gc_controller.h"
#include "keyboard_input.h"
#include "video/emuwindow.h"

namespace input_common {

bool KeyboardInput::SetControllerStatus(int channel, int key, GCController::GCButtonState state)
{
    if (common::g_config->controller_ports(channel).enable == false ||
        common::g_config->controller_ports(channel).keys.enable == false)
        return false;

    for (unsigned int i = 0; i < common::Config::NUM_CONTROLS; ++i)
    {
        if (key == common::g_config->controller_ports(channel).keys.key_code[i])
        {
            g_controller_state[channel]->set_control_status((common::Config::Control)i, state);

            // Set Main Stick and C-Stick Values Manually
            if(state == GCController::PRESSED)
            {
                common::Config::Control temp_control;
		temp_control = (common::Config::Control)i;

                switch(temp_control)
                {
                    case common::Config::ANALOG_UP:
                        g_controller_state[0]->ANALOG_Y = 0xE0;
                        break;
                    case common::Config::ANALOG_DOWN:
                        g_controller_state[0]->ANALOG_Y = 0x20;
                        break;
                    case common::Config::ANALOG_LEFT:
                        g_controller_state[0]->ANALOG_X = 0x20;
                        break;
                    case common::Config::ANALOG_RIGHT:
                        g_controller_state[0]->ANALOG_X = 0xE0;
                        break;
                    case common::Config::C_UP:
                        g_controller_state[0]->C_Y = 0xE0;
                        break;
                    case common::Config::C_DOWN:
                        g_controller_state[0]->C_Y = 0x20;
                        break;
                    case common::Config::C_LEFT:
                        g_controller_state[0]->C_X = 0x20;
                        break;
                    case common::Config::C_RIGHT:
                        g_controller_state[0]->C_X = 0xE0;
                        break;
                    default:
                        break;
                } 
            }

            return true;
        }
    }
    return false;
}

GCController::GCButtonState KeyboardInput::GetControllerStatus(int channel, int key)
{
    for (unsigned int i = 0; i < common::Config::NUM_CONTROLS; ++i)
    {
        if (key == common::g_config->controller_ports(channel).keys.key_code[i])
        {
            return g_controller_state[channel]->control_status((common::Config::Control)i);
        }
    }

    // Return GC_CONTROLLER_NULL if unmapped pad
    return GCController::GC_CONTROLLER_NULL;   
}

void KeyboardInput::PollEvents() {
    emuwindow_->PollEvents();
}

void KeyboardInput::ShutDown() {
}

bool KeyboardInput::Init() {
    g_controller_state[0]->set_input_type(GCController::KEYBOARD_INPUT);
    return true;
}

KeyboardInput::KeyboardInput(EmuWindow* emu_window) : emuwindow_(emu_window)
{
    emuwindow_->set_controller_interface(this);
}

KeyboardInput::~KeyboardInput()
{
    emuwindow_->set_controller_interface(NULL);
}



} // namespace
