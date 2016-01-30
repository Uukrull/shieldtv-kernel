/* Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include "Nvs.h"
#include "NvsAccel.h"
#include "NvsAnglvel.h"
#include "NvsIlluminance.h"
#include "NvsMagn.h"
#include "NvsPressure.h"
#include "NvsProximity.h"
#include "NvsTemp.h"
#include "NvsInv.h"

/* possible SENSOR_COUNT_MAX =
 * SENSOR_TYPE_ACCELEROMETER
 * SENSOR_TYPE_GEOMAGNETIC_FIELD
 * SENSOR_TYPE_ORIENTATION
 * SENSOR_TYPE_GYROSCOPE
 * SENSOR_TYPE_LIGHT
 * SENSOR_TYPE_PRESSURE
 * SENSOR_TYPE_PROXIMITY
 * SENSOR_TYPE_GRAVITY
 * SENSOR_TYPE_LINEAR_ACCELERATION
 * SENSOR_TYPE_ROTATION_VECTOR
 * SENSOR_TYPE_AMBIENT_TEMPERATURE (FROM BMPX80)
 * SENSOR_TYPE_AMBIENT_TEMPERATURE (FROM MPU FOR GYRO)
 * SENSOR_TYPE_GAME_ROTATION_VECTOR
 * SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED
 * SENSOR_TYPE_GYROSCOPE_UNCALIBRATED
 * SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR
 */
/* The following is only available if using a sensor hub.
 * SENSOR_TYPE_SIGNIFICANT_MOTION
 * SENSOR_TYPE_STEP_DETECTOR
 * SENSOR_TYPE_STEP_COUNTER
 */
#define SENSOR_COUNT_MAX                19
#define SENSOR_PLATFORM_NAME            "T210 sensors module"
#define SENSOR_PLATFORM_AUTHOR          "NVIDIA CORP"
#define SENSORS_DEVICE_API_VERSION      SENSORS_DEVICE_API_VERSION_1_3

static struct sensor_t nvspSensorList[SENSOR_COUNT_MAX] = {
};

static NvsIlluminance *mNvsIlluminance = NULL;
static NvsAccel *mNvsAccel = NULL;
static NvsAnglvel *mNvsAnglvel = NULL;
static NvsTemp *mNvsGyroTemp = NULL;
static NvsPressure *mNvsPressure = NULL;
static NvsMagn *mNvsMagn = NULL;

static SensorBase *newIlluminance(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    mNvsIlluminance = new NvsIlluminance(devNum, driverType);
    return mNvsIlluminance;
}

static SensorBase *newProximity(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    return new NvsProximity(devNum, driverType, mNvsIlluminance);
}

static SensorBase *newAccel(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    mNvsAccel = new NvsAccel(devNum, driverType);
    return mNvsAccel;
}

static SensorBase *newAnglvel(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    mNvsAnglvel = new NvsAnglvel(devNum, driverType, mNvsAccel);
    return mNvsAnglvel;
}

static SensorBase *newGyroTemp(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    mNvsGyroTemp = new NvsTemp(devNum, driverType, mNvsAnglvel);
    return mNvsGyroTemp;
}

static SensorBase *newPressure(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    mNvsPressure = new NvsPressure(devNum, driverType);
    return mNvsPressure;
}

static SensorBase *newPresTemp(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    return new NvsTemp(devNum, driverType, mNvsPressure);
}

static SensorBase *newMagn(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    mNvsMagn = new NvsMagn(devNum, driverType);
    return mNvsMagn;
}

static SensorBase *newFusion(int devNum, enum NVS_DRIVER_TYPE driverType)
{
    if (mNvsAccel) {
        if (!mNvsAccel->getSensorList(NULL, 0, 0))
            mNvsAccel = NULL;
    }
    if (mNvsAnglvel) {
        if (!mNvsAnglvel->getSensorList(NULL, 0, 0))
            mNvsAnglvel = NULL;
    }
    if (mNvsGyroTemp) {
        if (!mNvsGyroTemp->getSensorList(NULL, 0, 0))
            mNvsGyroTemp = NULL;
    }
    if (mNvsMagn) {
        if (!mNvsMagn->getSensorList(NULL, 0, 0))
        mNvsMagn = NULL;
    }
    return new NvsInv(mNvsAccel, mNvsAnglvel, mNvsGyroTemp, mNvsMagn);
}

static struct NvspDriver nvspDriverList[] = {
    {
        .devName                = "jsa1127",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newIlluminance,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "cm3217",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newIlluminance,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "max4400x",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newIlluminance,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "max4400x",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newProximity,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "bmpX80",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newPressure,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "bmpX80",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newPresTemp,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "ak89xx",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newMagn,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "magnetic_field",
        .devType                = SENSOR_TYPE_GEOMAGNETIC_FIELD,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newMagn,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "mpu6xxx",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newAccel,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "mpu6xxx",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newAnglvel,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "mpu6xxx",
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = true,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newGyroTemp,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    /* the fusion driver must be last to link to fusion devices */
    {
        .devName                = "mpu6xxx", /* fusion driver only if INV */
        .devType                = 0,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = true,
        .newDriver              = &newFusion,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },


    /* This list uses the auto HAL driver mechanism.
     * It's used for NVS kernel drivers that have been converted to this
     * mechanism as well as used by the sensor hub.
     */
    {
        .devName                = "accelerometer",
        .devType                = SENSOR_TYPE_ACCELEROMETER,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "orientation",
        .devType                = SENSOR_TYPE_ORIENTATION,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "gyroscope",
        .devType                = SENSOR_TYPE_GYROSCOPE,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "light",
        .devType                = SENSOR_TYPE_LIGHT,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "pressure",
        .devType                = SENSOR_TYPE_PRESSURE,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "proximity",
        .devType                = SENSOR_TYPE_PROXIMITY,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "ambient_temperature",
        .devType                = SENSOR_TYPE_AMBIENT_TEMPERATURE,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "rotation_vector",
        .devType                = SENSOR_TYPE_ROTATION_VECTOR,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "step_detector",
        .devType                = SENSOR_TYPE_STEP_DETECTOR,
        .driverType             = NVS_DRIVER_TYPE_IIO,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = NULL,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
};

#include "Nvsp.h"

