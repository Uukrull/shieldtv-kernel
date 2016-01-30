/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2015, NVIDIA Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/input.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <cutils/properties.h>

#include "common.h"
#include "device.h"
#include "screen_ui.h"
#include "ui.h"

const char* HEADERS_ers[] = { "Volume up/down to move highlight;",
                              "enter/power button to select.",
                              "",
                              NULL };

const char* HEADERS_loki[] = { "Press Up/Down(Keyboard), X/Y(Joystick) or Power Button to move highlight;",
                               "Press Enter(Keyboard), A(Joystick) or Power Button for 4-6s to select;",
                               "Press Shift(Keyboard), B(Joystick) to toggle recovery menu;",
                               "On Shield Android TV - USB connection is required to use SHIELD controller.",
                               "",
                               NULL };

const char* ITEMS[] = { "reboot system now",
                        "apply update from ADB",
                        "wipe data/factory reset",
                        "wipe cache partition",
                        NULL };

char platform[PROPERTY_VALUE_MAX+1];

class T210RecoveryUI : public ScreenRecoveryUI {
  public:
    T210RecoveryUI() :
        consecutive_power_keys(0) {
        property_get("ro.hardware", platform, "");
    }

    virtual KeyAction CheckKey(int key) {
            if (IsKeyPressed(KEY_POWER) && key == KEY_HOME) {
                return TOGGLE;
            }

	if (key == KEY_TOGGLE) {
		return TOGGLE;
	}

        if (key == KEY_POWER) {
            ++consecutive_power_keys;
            if (consecutive_power_keys >= 7) {
                return REBOOT;
            }
        } else {
            consecutive_power_keys = 0;
        }
        return ENQUEUE;
    }

  private:
    int consecutive_power_keys;
};


class T210Device : public Device {
  public:
    T210Device() :
        ui(new T210RecoveryUI) {
    }

    T210RecoveryUI* GetUI() { return ui; }

    int HandleMenuKey(int key_code, int visible) {
        if (visible) {
            switch (key_code) {
              case KEY_BACK:
              case BTN_X:
              case KEY_DOWN:
              case KEY_VOLUMEDOWN:
              case KEY_SCROLL:
                return kHighlightDown;

              case BTN_Y:
              case KEY_UP:
              case KEY_VOLUMEUP:
                return kHighlightUp;

              case KEY_POWER:
              case KEY_ENTER:
              case BTN_A:
              case KEY_INVOKE:
                return kInvokeItem;
            }
        }

        return kNoAction;
    }

    BuiltinAction InvokeMenuItem(int menu_position) {
        switch (menu_position) {
          case 0: return REBOOT;
          case 1: return APPLY_ADB_SIDELOAD;
          case 2: return WIPE_DATA;
          case 3: return WIPE_CACHE;
          default: return NO_ACTION;
        }
    }

    const char* const* GetMenuHeaders() {
        if (!strncmp(platform, "loki_e", 6) ||
            !strncmp(platform, "foster_e", 8)) {
            return HEADERS_loki;
        }
        return HEADERS_ers;
    }

    const char* const* GetMenuItems() { return ITEMS; }

  private:
    T210RecoveryUI* ui;
};

Device* make_device() {
    return new T210Device;
}
