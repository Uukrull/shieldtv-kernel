/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include "powerhal.h"
#ifdef ENABLE_SATA_STANDBY_MODE
#include "tegra_sata_hal.h"
#endif

#define FOSTER_E_HDD    "/dev/block/sda"
#define HDD_STANDBY_TIMEOUT     60

static struct powerhal_info *pInfo;
static struct input_dev_map input_devs[] = {
        {-1, "touch\n"},
       };

/*
 * The order in camera_cap array should match with
 * use case order in camera_usecase_t.
 * if min_online_cpus or max_online_cpus is zero, then
 * it won't be applied.
 * Freq is in KHz.
 */
static camera_cap_t camera_cap[] = {
    /* still preview
     * {min_online_cpus, max_online_cpus, freq, minFreq,
     *  minCpuHint, maxCpuHint, minGpuHint, maxGpuHint, fpsHint}
    */
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* video preview
     * {min_online_cpus, max_online_cpus, freq, minFreq,
     *  minCpuHint, maxCpuHint, minGpuHint, maxGpuHint, fpsHint}
    */
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* video record
     * {min_online_cpus, max_online_cpus, freq, minFreq,
     *  minCpuHint, maxCpuHint, minGpuHint, maxGpuHint, fpsHint}
    */
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* high fps video record
     * {min_online_cpus, max_online_cpus, freq, minFreq,
     *  minCpuHint, maxCpuHint, minGpuHint, maxGpuHint, fpsHint}
    */
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static void loki_e_power_init(struct power_module *module)
{
    if (!pInfo)
        pInfo = (powerhal_info*)malloc(sizeof(powerhal_info));
    pInfo->input_devs = input_devs;
    pInfo->input_cnt = sizeof(input_devs)/sizeof(struct input_dev_map);

    common_power_init(module, pInfo);
}

static void loki_e_power_set_interactive(struct power_module *module, int on)
{
    int error = 0;

    common_power_set_interactive(module, pInfo, on);

#ifdef ENABLE_SATA_STANDBY_MODE
    if (!access(FOSTER_E_HDD, F_OK)) {
        /*
        * Turn-off Foster HDD at display off
        */
        ALOGI("HAL: Display is %s, set HDD to %s standby mode.", on?"on":"off", on?"disable":"enter");
        if (on) {
            error = hdd_disable_standby_timer();
            if (error)
                ALOGE("HAL: Failed to set standby timer, error: %d", error);
        }
        else {
            error = hdd_set_standby_timer(HDD_STANDBY_TIMEOUT);
            if (error)
                ALOGE("HAL: Failed to set standby timer, error: %d", error);
        }
    }
#endif
}

static void loki_e_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    common_power_hint(module, pInfo, hint, data);
}

static int loki_e_power_open(const hw_module_t *module, const char *name,
                            hw_device_t **device)
{
    if (strcmp(name, POWER_HARDWARE_MODULE_ID))
        return -EINVAL;

    if (!pInfo) {
        pInfo = (powerhal_info*)calloc(1, sizeof(powerhal_info));

        common_power_open(pInfo);
        common_power_camera_init(pInfo, camera_cap);
    }

    return 0;
}

static struct hw_module_methods_t power_module_methods = {
    open: loki_e_power_open,
};

struct power_module HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: POWER_MODULE_API_VERSION_0_2,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: POWER_HARDWARE_MODULE_ID,
        name: "Loki-E Power HAL",
        author: "NVIDIA",
        methods: &power_module_methods,
        dso: NULL,
        reserved: {0},
    },

    init: loki_e_power_init,
    setInteractive: loki_e_power_set_interactive,
    powerHint: loki_e_power_hint,
};
