/*
 * Copyright (C) 2011 The Android Open Source Project
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
#include <fcntl.h>

#include <fs_mgr.h>
#include "roots.h"
#include "common.h"
#include "device.h"
#include "screen_ui.h"
#include "make_ext4fs.h"
extern "C" {
#include "wipe.h"
}

static const char* HEADERS[] = { "Volume up/down to move highlight;",
                                 "enter button to select.",
                                 "",
                                 NULL };

static const char* ITEMS[] =  {"reboot system now",
                               "apply update from ADB",
                               "wipe data/factory reset",
                               "wipe cache partition",
                               "reboot to bootloader",
                               "power down",
                               NULL };

static const char* USERCALIB_PATH = "/mnt/usercalib";

int erase_usercalibration_partition() {
    Volume *v = volume_for_path(USERCALIB_PATH);
    if (v == NULL) {
        // most devices won't have /mnt/usercalib, so this is not an error.
        return 0;
    }

    int fd = open(v->blk_device, O_RDWR);
    uint64_t size = get_file_size(fd);
    if (size != 0) {
        if (wipe_block_device(fd, size)) {
            LOGE("error wiping /mnt/usercalib: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
    }

    close(fd);

    return 0;
}

#ifndef PLATFORM_IS_NEXT
class DefaultUI : public ScreenRecoveryUI {
  public:
    virtual KeyAction CheckKey(int key) {
        if (key == KEY_HOME) {
            return TOGGLE;
        }
        return ENQUEUE;
    }
};
#endif PLATFORM_IS_NEXT

class DefaultDevice : public Device {
  public:
    DefaultDevice() :
#ifndef PLATFORM_IS_NEXT
        ui(new DefaultUI) {
    }
#else PLATFORM_IS_NEXT
        ui(new ScreenRecoveryUI) {
    }
#endif PLATFORM_IS_NEXT

    RecoveryUI* GetUI() { return ui; }

    int HandleMenuKey(int key, int visible) {
        if (visible) {
            switch (key) {
              case KEY_DOWN:
              case KEY_VOLUMEDOWN:
                return kHighlightDown;

              case KEY_UP:
              case KEY_VOLUMEUP:
                return kHighlightUp;

              case KEY_ENTER:
              case KEY_POWER:
                return kInvokeItem;
            }
        }

        return kNoAction;
    }

    BuiltinAction InvokeMenuItem(int menu_position) {
        switch (menu_position) {
#ifndef PLATFORM_IS_NEXT
          case 0: return REBOOT;
          case 1: return APPLY_ADB_SIDELOAD;
          case 2: return APPLY_EXT;
          case 3: return WIPE_DATA;
          case 4: return WIPE_CACHE;
          default: return NO_ACTION;
#else   PLATFORM_IS_NEXT
          case 0: return REBOOT;
          case 1: return APPLY_ADB_SIDELOAD;
          case 2: return WIPE_DATA;
          case 3: return WIPE_CACHE;
          case 4: return REBOOT_BOOTLOADER;
          case 5: return SHUTDOWN;
          default: return NO_ACTION;
#endif PLATFORM_IS_NEXT
        }
    }

    int WipeData() {
        erase_usercalibration_partition();
        return 0;
    }

    const char* const* GetMenuHeaders() { return HEADERS; }
    const char* const* GetMenuItems() { return ITEMS; }

  private:
    RecoveryUI* ui;
};

Device* make_device() {
    return new DefaultDevice();
}

