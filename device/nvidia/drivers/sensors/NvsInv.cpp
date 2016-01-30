/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <cutils/log.h>
#include <string.h>
#include <linux/input.h>
#include <utils/Atomic.h>

#include "NvsDev.h"
#include "NvsInv.h"

#include "invensense.h"
#include "invensense_adv.h"
#include "ml_stored_data.h"
#include "ml_load_dmp.h"
#include "ml_sysfs_helper.h"


static signed char matrixDisable[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

NvsInv::NvsInv(NvsAccel *accel,
               NvsAnglvel *anglvel,
               NvsTemp *gyroTemp,
               NvsMagn *magn)
               : SensorBase(NULL, NULL),
                 mAccelSensor(accel),
                 mGyroSensor(anglvel),
                 mGyroTempSensor(gyroTemp),
                 mCompassSensor(magn),
                 mHaveGoodMpuCal(0),
                 mGyroAccuracy(0),
                 mAccelAccuracy(0),
                 mCompassAccuracy(0),
                 mEnabled(0),
                 mHasPendingEvent(false),
                 mAccelScale(2),
                 mAccelSelfTestScale(2),
                 mGyroScale(2000),
                 mGyroSelfTestScale(250),
                 mCompassScale(0),
                 mFactoryGyroBiasAvailable(false),
                 mGyroBiasAvailable(false),
                 mGyroBiasApplied(false),
                 mFactoryAccelBiasAvailable(false),
                 mAccelBiasAvailable(false),
                 mAccelBiasApplied(false)
{
    VFUNC_LOG;

    inv_error_t rv;
    int i, fd;
    char path[MAX_SYSFS_NAME_LEN];
    char *ver_str;
    int res;
    FILE *fptr;

    ALOGI_IF(EXTRA_VERBOSE,
             "HAL:NvsInv constructor : MpuNumSensors = %d", MpuNumSensors);

    ALOGI_IF(ENG_VERBOSE, "%s\nmAccelSensor=%p\nmGyroSensor=%p\nmGyroTempSensor=%p\n"
             "mCompassSensor=%p\n\n",
             __func__, mAccelSensor, mGyroSensor, mGyroTempSensor,
             mCompassSensor);

    memset(mGyroOrientation, 0, sizeof(mGyroOrientation));
    memset(mAccelOrientation, 0, sizeof(mAccelOrientation));
    /* initalize SENSOR_TYPE_META_DATA */
    memset(&mMetaDataEvent, 0, sizeof(mMetaDataEvent));
    mMetaDataEvent.version = META_DATA_VERSION;
    mMetaDataEvent.type = SENSOR_TYPE_META_DATA;
    for (i = 0; i < MpuNumSensors; i++) {
        mSensorList[i] = NULL;
        mSamplePeriodHw[i] = 0LL;
        mSamplePeriodNs[i] = 1000000000LL;
        mMaxReportLatencyNs[i] = 0LL;
    }
    /* get chip name */
//    sprintf(path, "%s/part", mSysfsPath);
//    res = sysfsStrRead(path, chip_ID);
//    if (res > 0)
//        ALOGI("%s %s Chip ID = %s\n", __func__, path, chip_ID);
//    else
//        ALOGE("%s ERR: %d: Failed to get chip ID from %s\n",
//              __func__, res, path);

    if (mGyroSensor) {
        /* read gyro FSR to calculate accel scale later */
        float range;
        res = mGyroSensor->getMaxRange(-1, &range);
        if (!res) {
            if (range > 30)
                mGyroScale = 2000;
            else if (range > 15)
                mGyroScale = 1000;
            else if (range > 7)
                mGyroScale = 500;
            else if (range > 3)
                mGyroScale = 250;
        }
        ALOGI_IF(EXTRA_VERBOSE, "%s: Gyro FSR used %d", __func__, mGyroScale);
    }

    /* mFactoryGyBias contains bias values that will be used for device offset */
    memset(mFactoryGyroBias, 0, sizeof(mFactoryGyroBias));

    /* open Gyro Bias fd */
    /* mGyroBias contains bias values that will be used for framework */
    /* mGyroChipBias contains bias values that will be used for dmp */
    memset(mGyroBias, 0, sizeof(mGyroBias));
    memset(mGyroChipBias, 0, sizeof(mGyroChipBias));

    if (mAccelSensor) {
        /* read accel FSR to calculate accel scale later */
        mSensorList[Accelerometer] = mAccelSensor->getSensorListPtr();
        if (mSensorList[Accelerometer]) {
            if (mSensorList[Accelerometer]->version > 2) {
                float range = 0;
                mAccelSensor->getMaxRange(-1, &range);
                if (range > 120)
                    mAccelScale = 16;
                else if (range > 60)
                    mAccelScale = 8;
                else if (range > 30)
                    mAccelScale = 4;
                else if (range > 15)
                    mAccelScale = 2;
            }
        }
        ALOGI_IF(EXTRA_VERBOSE, "%s: Accel FSR used %d", __func__, mAccelScale);
    }
    /* read accel self test scale used to calculate factory cal bias later */
    if (!strcmp(chip_ID, "mpu6050"))
        /* initialized to 2 as MPU65xx default*/
        mAccelSelfTestScale = 8;

    /* mFactoryAccelBias contains bias values that will be used for device offset */
    memset(mFactoryAccelBias, 0, sizeof(mFactoryAccelBias));
    /* open Accel Bias fd */
    /* mAccelBias contains bias that will be used for dmp */
    memset(mAccelBias, 0, sizeof(mAccelBias));

    initBias();

    (void)inv_get_version(&ver_str);
    ALOGI("%s\n", ver_str);

    /* setup MPL */
    inv_constructor_init();

    /* setup orientation matrix and scale */
    inv_set_device_properties();

    /* initialize sensor data */
    memset(mPendingEvents, 0, sizeof(mPendingEvents));
    for (i = 0; i < MpuNumSensors; i++) {
        mPendingEvents[i].version = sizeof(sensors_event_t);
        mPendingEvents[i].type = sMplSensorList[i].type;
        mPendingEvents[i].acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
    }
    /* Event Handlers for HW and Virtual Sensors */
    mHandlers[RotationVector] = &NvsInv::HandlerRotationVector;
    mHandlers[GameRotationVector] = &NvsInv::HandlerGameRotationVector;
    mHandlers[LinearAccel] = &NvsInv::HandlerLinearAccel;
    mHandlers[Gravity] = &NvsInv::HandlerGravity;
    mHandlers[Gyro] = &NvsInv::HandlerGyro;
    mHandlers[RawGyro] = &NvsInv::HandlerRawGyro;
    mHandlers[Accelerometer] = &NvsInv::HandlerAccelerometer;
    mHandlers[MagneticField] = &NvsInv::HandlerMagneticField;
    mHandlers[RawMagneticField] = &NvsInv::HandlerRawMagneticField;
    mHandlers[Orientation] = &NvsInv::HandlerOrientation;
    mHandlers[GeomagneticRotationVector] = &NvsInv::HandlerGeomagneticRotationVector;
    /* initialize Compass Bias */
    memset(mCompassBias, 0, sizeof(mCompassBias));
    /* initialize Factory Accel Bias */
    memset(mFactoryAccelBias, 0, sizeof(mFactoryAccelBias));
    /* initialize Gyro Bias */
    memset(mGyroBias, 0, sizeof(mGyroBias));
    memset(mGyroChipBias, 0, sizeof(mGyroChipBias));
    /* load calibration file from /data/inv_cal_data.bin */
    rv = inv_load_calibration();
    if(rv == INV_SUCCESS) {
        ALOGI_IF(PROCESS_VERBOSE, "HAL:Calibration file successfully loaded");
        /* Get initial values */
        getCompassBias();
        getGyroBias();
        if (mGyroBiasAvailable) {
            setGyroBias();
        }
        getAccelBias();
        getFactoryGyroBias();
        if (mFactoryGyroBiasAvailable) {
            setFactoryGyroBias();
        }
        getFactoryAccelBias();
        if (mFactoryAccelBiasAvailable) {
            setFactoryAccelBias();
        }
    }
    else
        ALOGE("HAL:Could not open or load MPL calibration file (%d)", rv);


    /* disable all sensors and features */
//    enableGyro(0);
//    enableAccel(0);
//    enableCompass(0,0);
    if (mAccelSensor) {
        if (mAccelSensor->getEnable(-1))
            mEnabled |= (1 << Accelerometer);
    }
    if (mGyroSensor) {
        if (mGyroSensor->getEnable(-1))
            mEnabled |= (1 << Gyro);
    }
    if (mCompassSensor) {
        if (mCompassSensor->getEnable(-1))
            mEnabled |= (1 << MagneticField);
    }
}

int NvsInv::inv_constructor_init(void)
{
    VFUNC_LOG;

    inv_error_t result = inv_init_mpl();
    if (result) {
        ALOGE("HAL:inv_init_mpl() failed");
        return result;
    }
    result = inv_constructor_default_enable();
    result = inv_start_mpl();
    if (result) {
        ALOGE("HAL:inv_start_mpl() failed");
        LOG_RESULT_LOCATION(result);
        return result;
    }

    return result;
}

int NvsInv::inv_constructor_default_enable(void)
{
    VFUNC_LOG;

    inv_error_t result;

/*******************************************************************************

********************************************************************************

The InvenSense binary file (libmplmpu.so) is subject to Google's standard terms
and conditions as accepted in the click-through agreement required to download
this library.
The library includes, but is not limited to the following function calls:
inv_enable_quaternion().

ANY VIOLATION OF SUCH TERMS AND CONDITIONS WILL BE STRICTLY ENFORCED.

********************************************************************************

*******************************************************************************/

    result = inv_enable_quaternion();
    if (result) {
        ALOGE("HAL:Cannot enable quaternion\n");
        return result;
    }

    result = inv_enable_in_use_auto_calibration();
    if (result) {
        return result;
    }

    result = inv_enable_fast_nomot();
    if (result) {
        return result;
    }

    result = inv_enable_gyro_tc();
    if (result) {
        return result;
    }

    result = inv_enable_hal_outputs();
    if (result) {
        return result;
    }

    if (mCompassSensor != NULL) {
        /* Invensense compass calibration */
        ALOGI_IF(ENG_VERBOSE, "HAL:Invensense vector compass cal enabled");
        result = inv_enable_vector_compass_cal();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }
        // specify MPL's trust weight, used by compass algorithms
        inv_vector_compass_cal_sensitivity(3);

        /* disabled by default
        result = inv_enable_compass_bias_w_gyro();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }
        */

        result = inv_enable_heading_from_gyro();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }

        result = inv_enable_magnetic_disturbance();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }
        //inv_enable_magnetic_disturbance_logging();
    }

    result = inv_enable_9x_sensor_fusion();
    if (result) {
        LOG_RESULT_LOCATION(result);
        return result;
    }

    result = inv_enable_no_gyro_fusion();
    if (result) {
        LOG_RESULT_LOCATION(result);
        return result;
    }

    return result;
}

/* TODO: create function pointers to calculate scale */
void NvsInv::inv_set_device_properties(void)
{
    VFUNC_LOG;

    unsigned short orient;

    /* get gyro orientation */
    if (mGyroSensor != NULL) {
        mGyroSensor->getMatrix(0, mGyroOrientation);
        /* disable NVS matrix once fusion has it */
        mGyroSensor->setMatrix(0, matrixDisable);
    }

    /* get accel orientation */
    if (mAccelSensor != NULL) {
        mAccelSensor->getMatrix(0, mAccelOrientation);
        /* disable NVS matrix once fusion has it */
        mAccelSensor->setMatrix(0, matrixDisable);
    }

    inv_set_gyro_sample_rate(DEFAULT_MPL_GYRO_RATE);
    inv_set_compass_sample_rate(DEFAULT_MPL_COMPASS_RATE);

    /* gyro setup */
    orient = inv_orientation_matrix_to_scalar(mGyroOrientation);
    inv_set_gyro_orientation_and_scale(orient, mGyroScale << 15);
    ALOGI_IF(EXTRA_VERBOSE, "HAL: Set MPL Gyro Scale %ld", mGyroScale << 15);

    /* accel setup */
    orient = inv_orientation_matrix_to_scalar(mAccelOrientation);
    /* use for third party accel input subsystem driver
    inv_set_accel_orientation_and_scale(orient, 1LL << 22);
    */
    inv_set_accel_orientation_and_scale(orient, (long)mAccelScale << 15);
    ALOGI_IF(EXTRA_VERBOSE,
             "HAL: Set MPL Accel Scale %ld", (long)mAccelScale << 15);

    /* compass setup */
    if (mCompassSensor != NULL) {
        signed char orientMtx[9];
        mCompassSensor->getMatrix(0, orientMtx);
        /* disable NVS matrix once fusion has it */
        mCompassSensor->setMatrix(0, matrixDisable);
        orient = inv_orientation_matrix_to_scalar(orientMtx);
        float scale = 0;
        mCompassSensor->getScale(-1, &scale);
        long sensitivity = (long)(scale * (1L << 30));
        inv_set_compass_orientation_and_scale(orient, sensitivity);
        mCompassScale = sensitivity;
        ALOGI_IF(EXTRA_VERBOSE,
                 "HAL: Set MPL Compass Scale %ld", mCompassScale);
    }
}

NvsInv::~NvsInv()
{
    VFUNC_LOG;
}

int NvsInv::getSensorList(struct sensor_t *list, int index, int limit)
{
    VFUNC_LOG;

    int index_start;
    int i;

    if (list == NULL) /* just get sensor count */
        return ARRAY_SIZE(sMplSensorList);

    if (mAccelSensor != NULL) {
        mSensorList[Accelerometer] = mAccelSensor->getSensorListPtr();
        if (mSensorList[Accelerometer] == NULL) {
            ALOGE("%s ERR: no accelerometer in sensor list\n", __func__);
            mAccelSensor = NULL;
        } else {
            mPendingEvents[Accelerometer].sensor =
                                            mSensorList[Accelerometer]->handle;
            if (mSensorList[Accelerometer]->version < 3) {
                /* FIXME: REMOVE ME */
                mSensorList[Accelerometer]->maxRange *= GRAVITY_EARTH;
                mSensorList[Accelerometer]->resolution *= GRAVITY_EARTH;
            }
//            if (!mDmpLoaded) {
                mSensorList[Accelerometer]->fifoReservedEventCount = 0;
                mSensorList[Accelerometer]->fifoMaxEventCount = 0;
//            }
        }
    }
    if (mGyroSensor != NULL) {
        mSensorList[Gyro] = mGyroSensor->getSensorListPtr();
        if (mSensorList[Gyro] == NULL) {
            ALOGE("%s ERR: no gyro device in sensor list\n", __func__);
            mGyroSensor = NULL;
        } else {
            mPendingEvents[Gyro].sensor = mSensorList[Gyro]->handle;
//            if (!mDmpLoaded) {
                mSensorList[Gyro]->fifoReservedEventCount = 0;
                mSensorList[Gyro]->fifoMaxEventCount = 0;
//            }
        }
    }
    if (mCompassSensor != NULL) {
        mSensorList[MagneticField] = mCompassSensor->getSensorListPtr();
        if (mSensorList[MagneticField] == NULL) {
            ALOGE("%s ERR: no compass device in sensor list\n", __func__);
            mCompassSensor = NULL;
        } else {
            mPendingEvents[MagneticField].sensor =
                                            mSensorList[MagneticField]->handle;
//            if (!mDmpLoaded) {
                mSensorList[MagneticField]->fifoReservedEventCount = 0;
                mSensorList[MagneticField]->fifoMaxEventCount = 0;
//            }
        }
    }
    index_start = index;
    if ((index - index_start) >= limit)
        return index - index_start;

    /* the following needs gyro */
    if (mSensorList[Gyro]) {
        /* copy from list for the raw gyro sensor */
        memcpy(&list[index], mSensorList[Gyro], sizeof(struct sensor_t));
        list[index].type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
        list[index].handle = index + 1;
        mPendingEvents[RawGyro].sensor = list[index].handle;
        mSensorList[RawGyro] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    /* the following needs compass */
    if (mSensorList[MagneticField]) {
        /* copy from list for the raw compass sensor */
        memcpy(&list[index], mSensorList[MagneticField],
               sizeof(struct sensor_t));
        list[index].type = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
        list[index].handle = index + 1;
        mPendingEvents[RawMagneticField].sensor = list[index].handle;
        mSensorList[RawMagneticField] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    /* the virtual sensors need a combination of the physical sensors */
    if (mSensorList[Accelerometer] && mSensorList[Gyro] &&
                                                  mSensorList[MagneticField]) {

        memcpy(&list[index], &sMplSensorList[Orientation],
               sizeof(struct sensor_t));
        list[index].power = mSensorList[Gyro]->power +
                            mSensorList[Accelerometer]->power +
                            mSensorList[MagneticField]->power;
        list[index].handle = index + 1;
        mPendingEvents[Orientation].sensor = list[index].handle;
        mSensorList[Orientation] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[RotationVector],
               sizeof(struct sensor_t));
        list[index].power = mSensorList[Gyro]->power +
                            mSensorList[Accelerometer]->power +
                            mSensorList[MagneticField]->power;
        list[index].handle = index + 1;
        mPendingEvents[RotationVector].sensor = list[index].handle;
        mSensorList[RotationVector] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[LinearAccel],
               sizeof(struct sensor_t));
        list[index].power = mSensorList[Gyro]->power +
                            mSensorList[Accelerometer]->power +
                            mSensorList[MagneticField]->power;
        list[index].resolution = mSensorList[Accelerometer]->resolution;
        list[index].maxRange = mSensorList[Accelerometer]->maxRange;
        list[index].handle = index + 1;
        mPendingEvents[LinearAccel].sensor = list[index].handle;
        mSensorList[LinearAccel] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[Gravity],
               sizeof(struct sensor_t));
        list[index].power = mSensorList[Gyro]->power +
                            mSensorList[Accelerometer]->power +
                            mSensorList[MagneticField]->power;
        list[index].handle = index + 1;
        mPendingEvents[Gravity].sensor = list[index].handle;
        mSensorList[Gravity] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (mSensorList[Accelerometer] && mSensorList[MagneticField]) {
        memcpy(&list[index], &sMplSensorList[GeomagneticRotationVector],
               sizeof(struct sensor_t));
        list[index].power = mSensorList[Accelerometer]->power +
                            mSensorList[MagneticField]->power;
        list[index].handle = index + 1;
        mPendingEvents[GeomagneticRotationVector].sensor = list[index].handle;
        mSensorList[GeomagneticRotationVector] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (mSensorList[Accelerometer] && mSensorList[Gyro]) {
        memcpy(&list[index], &sMplSensorList[GameRotationVector],
               sizeof(struct sensor_t));
//        if (mDmpLoaded) {
            list[index].fifoReservedEventCount = 0;
            list[index].fifoMaxEventCount = 0;
//        }
        list[index].power = mSensorList[Gyro]->power +
                            mSensorList[Accelerometer]->power;
        list[index].handle = index + 1;
        mPendingEvents[GameRotationVector].sensor = list[index].handle;
        mSensorList[GameRotationVector] = &list[index];
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (PROCESS_VERBOSE) {
        for (i = 0; i < index; i++) {
            ALOGI("%s -------------- FUSION SENSOR %d --------------\n",
                  __func__, i);
            ALOGI("%s sensor_t[%d].name=%s",
                  __func__, i, list[i].name);
            ALOGI("%s sensor_t[%d].vendor=%s",
                  __func__, i, list[i].vendor);
            ALOGI("%s sensor_t[%d].version=%d\n",
                  __func__, i, list[i].version);
            ALOGI("%s sensor_t[%d].handle=%d\n",
                  __func__, i, list[i].handle);
            ALOGI("%s sensor_t[%d].type=%d\n",
                  __func__, i, list[i].type);
            ALOGI("%s sensor_t[%d].maxRange=%f\n",
                  __func__, i, list[i].maxRange);
            ALOGI("%s sensor_t[%d].resolution=%f\n",
                  __func__, i, list[i].resolution);
            ALOGI("%s sensor_t[%d].power=%f\n",
                  __func__, i, list[i].power);
            ALOGI("%s sensor_t[%d].minDelay=%d\n",
                  __func__, i, list[i].minDelay);
            ALOGI("%s sensor_t[%d].fifoReservedEventCount=%d\n",
                  __func__, i, list[i].fifoReservedEventCount);
            ALOGI("%s sensor_t[%d].fifoMaxEventCount=%d\n",
                  __func__, i, list[i].fifoMaxEventCount);
            /* SENSORS_DEVICE_API_VERSION_1_3 */
            ALOGI("%s sensor_t[%d].stringType=%s\n",
                  __func__, i, list[i].stringType);
            ALOGI("%s sensor_t[%d].requiredPermission=%s\n",
                  __func__, i, list[i].requiredPermission);
            ALOGI("%s sensor_t[%d].maxDelay=%d\n",
                  __func__, i, (int)list[i].maxDelay);
            ALOGI("%s sensor_t[%d].flags=%u\n",
                  __func__, i, (unsigned int)list[i].flags);
        }
        for (i = 0; i < MpuNumSensors; i++) {
            ALOGI("%s -------------- FUSION EVENT %d --------------\n",
                  __func__, i);
            ALOGI("%s sensors_event_t[%d].version=%d\n",
                  __func__, i, mPendingEvents[i].version);
            ALOGI("%s sensors_event_t[%d].sensor=%d\n",
                  __func__, i, mPendingEvents[i].sensor);
            ALOGI("%s sensors_event_t[%d].type=%d\n",
                  __func__, i, mPendingEvents[i].type);
        }
    }

    return index - index_start;
}

int NvsInv::getHandleIndex(int32_t handle)
{
    VFUNC_LOG;

    int snsrIndex;

    for (snsrIndex = 0; snsrIndex < MpuNumSensors; snsrIndex++) {
        if (mPendingEvents[snsrIndex].sensor == handle)
            break;
    }
    if (snsrIndex >= MpuNumSensors)
        return -1;

//    if (mSensorList[snsrIndex]) {
    //    ALOGI_IF(ENG_VERBOSE && snsrIndex >= 0, "HAL:getHandleIndex - snsrIndex=%d, sname=%s",
    //             snsrIndex, mSensorList[snsrIndex].stringType);
//    }

    return snsrIndex;
}

unsigned int NvsInv::getPhysSnsrMask(int snsrIndex)
{
    unsigned int physSnsrMask = 0;

    switch (snsrIndex) {
    case Gyro:
    case RawGyro:
        physSnsrMask = (1 << Gyro);
        break;

    case Accelerometer:
        physSnsrMask = (1 << Accelerometer);
        break;

    case MagneticField:
    case RawMagneticField:
        physSnsrMask = (1 << MagneticField);
        break;

    case Orientation:
    case RotationVector:
    case LinearAccel:
    case Gravity:
        physSnsrMask = ((1 << Accelerometer) | (1 << Gyro) | (1 << MagneticField));
        break;

    case GameRotationVector:
        physSnsrMask = ((1 << Accelerometer) | (1 << Gyro));
        break;

    case GeomagneticRotationVector:
        physSnsrMask = ((1 << Accelerometer) | (1 << MagneticField));
        break;

    default:
        break;
    }

    return physSnsrMask;
}

int NvsInv::enable(int32_t handle, int enable, int channel)
{
    bool store_cal = true;
    unsigned int physSnsrMask = 0;
    unsigned int changeMask = 0;
    unsigned int enableMask;
    int i;
    int ret;
    int ret_t = 0;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d enable=%d channel=%d\n",
             __func__, handle, enable, channel);
    int snsrIndex = getHandleIndex(handle);
    if (snsrIndex < 0) {
        ALOGE("%s ERR handle=%d\n", __func__, handle);
        return -EINVAL;
    }

    enableMask = mEnabled;
    if (enable)
        enableMask |= (1 << snsrIndex);
    else
        enableMask &= ~(1 << snsrIndex);
    /* physical sensors that were enabled */
    for (i = 0; i < MpuNumSensors; i++) {
        if (!(mEnabled & (1 << i)))
            continue;

        changeMask |= getPhysSnsrMask(i);
    }

    /* physical sensors that need to be enabled */
    for (i = 0; i < MpuNumSensors; i++) {
        if (!(enableMask & (1 << i)))
            continue;

        physSnsrMask |= getPhysSnsrMask(i);
    }

    /* which sensor's status changed */
    changeMask ^= physSnsrMask;
    /* enable/disable physical sensor */
    if (mAccelSensor && (changeMask & (1 << Accelerometer))) {
        ret = mAccelSensor->enable(0, !!(physSnsrMask & (1 << Accelerometer)));
        if (ret) {
            ret_t |= -EINVAL;
        } else {
            if (physSnsrMask & (1 << Accelerometer)) {
                ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                         "%s Accelerometer enabled\n", __func__);
            } else {
                ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                         "%s inv_accel_was_turned_off\n", __func__);
                inv_accel_was_turned_off();
                if (store_cal) {
                    storeCalibration();
                    store_cal = false;
                }
            }
        }
    }
    if (mGyroSensor && (changeMask & (1 << Gyro))) {
        ret = mGyroSensor->enable(0, !!(physSnsrMask & (1 << Gyro)));
        if (ret) {
            ret_t |= -EINVAL;
        } else {
            if (physSnsrMask & (1 << Gyro)) {
                ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                         "%s Gyro enabled\n", __func__);
            } else {
                ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                         "%s inv_gyro_was_turned_off\n", __func__);
                inv_gyro_was_turned_off();
                if (store_cal) {
                    storeCalibration();
                    store_cal = false;
                }
            }
        }
        /* also enable/disable the temperature for gyro calculations */
        if (mGyroTempSensor != NULL)
            mGyroTempSensor->enable(0, !!(physSnsrMask & (1 << Gyro)));
    }
    if (mCompassSensor && (changeMask & (1 << MagneticField))) {
        ret = mCompassSensor->enable(0, !!(physSnsrMask & (1 << MagneticField)));
        if (ret) {
            ret_t |= -EINVAL;
        } else {
            if (physSnsrMask & (1 << MagneticField)) {
                ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                         "%s MagneticField enabled\n", __func__);
            } else {
                ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                         "%s inv_compass_was_turned_off\n", __func__);
                inv_compass_was_turned_off();
                if (store_cal) {
                    storeCalibration();
                    store_cal = false;
                }
            }
        }
    }

//    if (mSensorList[snsrIndex]) {
    //    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
    //             "%s %s(handle=%d) enable=%d ret=%d\n",
    //             __func__, handle, mSensorList[snsrIndex].stringType, ret_t);
//    }

//    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
//             "%s mEnabled=%x enableMask=%x changeMask=%x physSnsrMask=%x handle=%d snsrIndex=%d enable=%d ret=%d\n",
//             __func__,mEnabled, enableMask, changeMask, physSnsrMask, handle, snsrIndex, enable, ret_t);
    if (!ret_t) {
        mEnabled = enableMask;
        batch(handle, 0, mSamplePeriodNs[snsrIndex],
              mMaxReportLatencyNs[snsrIndex]);
    }
    return ret_t;
}

/* Store calibration file */
void NvsInv::storeCalibration(void)
{
    VFUNC_LOG;

    if(mHaveGoodMpuCal || mAccelAccuracy >= 2 || mCompassAccuracy >= 3) {
       if (inv_store_calibration())
           ALOGE("%s: Cannot store calibration on file\n", __func__);
       else
           ALOGI_IF(PROCESS_VERBOSE,
                    "%s: calibration file updated\n", __func__);
    }
}

int NvsInv::flush(int handle)
{
    /* There are 4 possible ways to handle the flush:
     * 1. The device is disabled: return -EINVAL
     * 2. The device doesn't support batch: send a meta event at the next
     *    opportunity.
     * 3. The device is a virtual sensor:
     *    A. Send a flush to the physical sensor.
     *    B. When the physical sensor returns the meta event, intercept and
     *       send the flush corresponding to the virtual sensor.
     * 4. The device is a physical sensor:
     *    A. Send a flush to the physical sensor.
     *    B. Pass the physical sensor meta event upward.
     * Because of these possibilities there are 3 flag variables:
     * 1. mFlushSensor[MpuNumSensors]: physical sensors needed for each sensor
     * 2. mFlushPSensor: physical Sensors when flushed
     * 3. mFlush: pending sensor meta events
     */
    VFUNC_LOG;
    unsigned int physSnsrMask = 0;
    int ret;

    int snsrIndex = getHandleIndex(handle);
    if (snsrIndex < 0) {
        ALOGE("%s ERR handle=%d\n", __func__, handle);
        return -EINVAL;
    }

    if (!(mEnabled & (1 << snsrIndex)))
        /* return -EINVAL if not enabled */
        return -EINVAL;

    if (mSensorList[snsrIndex]) {
        if (!(mSensorList[snsrIndex]->fifoMaxEventCount)) {
            /* if batch mode isn't supported then we'll be sending a
             * meta event without a physical flush.
             */
            mFlush |= (1 << snsrIndex);
            return 0;
        }
    }

    /* get the physical sensor that needs the flush */
    physSnsrMask = getPhysSnsrMask(snsrIndex);
    mFlushSensor[snsrIndex] = physSnsrMask;
    if (mAccelSensor && (physSnsrMask & (1 << Accelerometer))) {
        ret = mAccelSensor->flush(handle);
        if (ret)
            mFlushSensor[snsrIndex] &= ~(1 << Accelerometer);
    }
    if (mGyroSensor && (physSnsrMask & (1 << Gyro))) {
        ret = mGyroSensor->flush(handle);
        if (ret)
            mFlushSensor[snsrIndex] &= ~(1 << Gyro);
    }
    if (mCompassSensor && (physSnsrMask & (1 << MagneticField))) {
        ret = mCompassSensor->flush(handle);
        if (ret)
            mFlushSensor[snsrIndex] &= ~(1 << MagneticField);
    }
    if (!mFlushSensor[snsrIndex])
        /* if physical sensor(s) flush failed then just send meta data */
        mFlush |= (1 << snsrIndex);
//FIXME: consider returning -EINVAL instead

    ALOGI("%s handle=%d snsrIndex=%d mFlushSensor[snsrIndex]=%u mFlush=%u mFlushPSensor=%u\n",
          __func__, handle, snsrIndex, mFlushSensor[snsrIndex], mFlush, mFlushPSensor);

    return 0;
}

int NvsInv::batch(int handle, int flags,
                  int64_t sampling_period_ns, int64_t max_report_latency_ns)
{
    int64_t minDelay = 0LL;
    int i;
    int ret;
    int ret_t = 0;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d flags=%d "
             "sampling_period_ns=%lld max_report_latency_ns=%lld\n", __func__,
             handle, flags, sampling_period_ns, max_report_latency_ns);
    int snsrIndex = getHandleIndex(handle);
    if (snsrIndex < 0) {
        ALOGE("%s ERR handle=%d\n", __func__, handle);
        return -EINVAL;
    }

// FIXME: do we want to get clever with individual sensors or
//        carpet bomb like INV?
//    if (!(mEnabled & (1 << snsrIndex)))
//        /* just store call if sensor not enabled */
//        return 0;

//    /* get all the sensors affected by this change */
//    unsigned int physSnsrMask = getPhysSnsrMask(snsrIndex);
//    physSnsrMask |= (1 << snsrIndex);

    /* get the fastest speed of enabled sensors */
    for (i = 0; i < MpuNumSensors; i++) {
        if (mEnabled & (1 << i)) {
            if (mSensorList[i]) {
                if (mSensorList[i]->minDelay > minDelay)
                    minDelay = mSensorList[i]->minDelay;
            }
            if (mSamplePeriodNs[i] && (mSamplePeriodNs[i] <
                                         sampling_period_ns))
                    sampling_period_ns = mSamplePeriodNs[i];
        }
    }
    /* test for minimum delay */
    minDelay *= 1000; /* us -> ns*/
    if (sampling_period_ns < minDelay)
        sampling_period_ns = minDelay;
    /* program physical sensors */
    if ((mEnabled & (1 << Accelerometer)) && (mSamplePeriodHw[Accelerometer] !=
                                             mSamplePeriodNs[Accelerometer])) {
        ret = mAccelSensor->batch(handle, flags,
                                  sampling_period_ns, max_report_latency_ns);
        if (ret)
            ret_t |= ret;
        else
            mSamplePeriodHw[Accelerometer] = mSamplePeriodNs[Accelerometer];
    }
    if ((mEnabled & (1 << Gyro)) && (mSamplePeriodHw[Gyro] !=
                                     mSamplePeriodNs[Gyro])) {
        ret = mGyroSensor->batch(handle, flags,
                                 sampling_period_ns, max_report_latency_ns);
        if (ret)
            ret_t |= ret;
        else
            mSamplePeriodHw[Gyro] = mSamplePeriodNs[Gyro];
    }
    if ((mEnabled & (1 << MagneticField)) && (mSamplePeriodHw[MagneticField] !=
                                              mSamplePeriodNs[MagneticField])) {
        ret = mCompassSensor->batch(handle, flags,
                                    sampling_period_ns, max_report_latency_ns);
        if (ret)
            ret_t |= ret;
        else
            mSamplePeriodHw[MagneticField] = mSamplePeriodNs[MagneticField];
    }
    mSamplePeriodNs[snsrIndex] = sampling_period_ns;
    mMaxReportLatencyNs[snsrIndex] = max_report_latency_ns;
    if (!ret_t) {
        sampling_period_ns /= 1000LL; /* ns -> us */
        inv_set_accel_sample_rate(sampling_period_ns);
        inv_set_gyro_sample_rate(sampling_period_ns);
        inv_set_compass_sample_rate(sampling_period_ns);
        inv_set_orientation_sample_rate(sampling_period_ns);
        inv_set_rotation_vector_sample_rate(sampling_period_ns);
        inv_set_rotation_vector_6_axis_sample_rate(sampling_period_ns);
        inv_set_orientation_geomagnetic_sample_rate(sampling_period_ns);
        inv_set_linear_acceleration_sample_rate(sampling_period_ns);
        inv_set_gravity_sample_rate(sampling_period_ns);
        inv_set_geomagnetic_rotation_vector_sample_rate(sampling_period_ns);
    }
    return ret_t;
}

int NvsInv::setDelay(int32_t handle, int64_t ns, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d ns=%lld channel=%d\n",
             __func__, handle, ns, channel);
    /* delay is obsolete and just falls through to batch with timeout = 0 */
    return batch(handle, 0, ns, 0);
}

int NvsInv::mpuEvent(sensors_event_t *data, int count, Nvs *sensor)
{
    VHANDLER_LOG;
    int n;
    int ret;

    if ((sensor == NULL) || (count < 1))
        return -1;

    n = sensor->readEvents(&mPendingEvent, 1);
    if (n > 0) {
        if (mPendingEvent.type) {
             /* if sensor data */
            if (mPendingEvent.type == SENSOR_TYPE_ACCELEROMETER) {
                mCachedAccelData[0] = (long)mPendingEvent.acceleration.x;
                mCachedAccelData[1] = (long)mPendingEvent.acceleration.y;
                mCachedAccelData[2] = (long)mPendingEvent.acceleration.z;
                inv_build_accel(mCachedAccelData, 0, mPendingEvent.timestamp);
                ALOGI_IF(INPUT_DATA,
                         "%s accel data: %+8ld %+8ld %+8ld - %lld", __func__,
                         mCachedAccelData[0], mCachedAccelData[1],
                         mCachedAccelData[2], mPendingEvent.timestamp);
                /* remember inital 6 axis quaternion */
                inv_time_t tempTimestamp;
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else if (mPendingEvent.type == SENSOR_TYPE_GYROSCOPE) {
                mCachedGyroData[0] = (short)mPendingEvent.gyro.x;
                mCachedGyroData[1] = (short)mPendingEvent.gyro.y;
                mCachedGyroData[2] = (short)mPendingEvent.gyro.z;
                inv_build_gyro(mCachedGyroData, mPendingEvent.timestamp);
                ALOGI_IF(INPUT_DATA,
                         "%s_gyro data: %+8d %+8d %+8d - %lld", __func__,
                         mCachedGyroData[0], mCachedGyroData[1],
                         mCachedGyroData[2], mPendingEvent.timestamp);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else if (mPendingEvent.type == SENSOR_TYPE_AMBIENT_TEMPERATURE) {
                long temperature = (long)mPendingEvent.temperature;
                ALOGI_IF(INPUT_DATA,
                         "%s raw temperature data: %ld - %lld", __func__,
                         temperature, mPendingEvent.timestamp);
                temperature <<= 16;
                float fVal;
                ret = mGyroTempSensor->getScale(-1, &fVal);
                if (!ret)
                    temperature *= fVal;
                ret = mGyroTempSensor->getOffset(-1, &fVal);
                if (!ret)
                    temperature += fVal;
                ALOGI_IF(INPUT_DATA,
                         "%s temperature data: %ld - %lld", __func__,
                         temperature, mPendingEvent.timestamp);
// INVENSENSE doesn't enable this in 5.3
//                inv_build_temp(temperature, mPendingEvent.timestamp);
                n = 0; /* data not sent since only used by fusion */
            } else {
// FIXME: Need to apply offset and scale to non-fusion-owned data with same FD
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        } else { /* sensors_meta_data_event_t */
            if (mPendingEvent.meta_data.sensor ==
                                        mPendingEvents[Accelerometer].sensor) {
                mFlushPSensor |= (1 << Accelerometer);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else if (mPendingEvent.meta_data.sensor ==
                                                 mPendingEvents[Gyro].sensor) {
                mFlushPSensor |= (1 << Gyro);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else {
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        }
    }
    return n;
}

int NvsInv::buildMpuEvent(sensors_event_t *data, int count)
{
    VHANDLER_LOG;
    int n = 0;
    int ret;

    if (count) {
        ret = mpuEvent(data, count, mAccelSensor);
        if (ret > 0) {
            n += ret;
            count -= ret;
        }
    }
    if (count) {
        ret = mpuEvent(data, count, mGyroSensor);
        if (ret > 0) {
            n += ret;
            count -= ret;
        }
    }
    if (count) {
        ret = mpuEvent(data, count, mGyroTempSensor);
        if (ret > 0) {
            n += ret;
            count -= ret;
        }
    }
    return n;
}

int NvsInv::buildCompassEvent(sensors_event_t *data, int count)
{
    VHANDLER_LOG;
    int n;

    if ((mCompassSensor == NULL) || (count < 1))
        return -1;

    n = mCompassSensor->readEvents(&mPendingEvent, 1);
    if (n > 0) {
        if (mPendingEvent.type) {
            if (mPendingEvent.type == SENSOR_TYPE_GEOMAGNETIC_FIELD) {
                mCachedCompassData[0] = (long)mPendingEvent.magnetic.x;
                mCachedCompassData[1] = (long)mPendingEvent.magnetic.y;
                mCachedCompassData[2] = (long)mPendingEvent.magnetic.z;
                mCompassTimestamp = mPendingEvent.timestamp;
                inv_build_compass(mCachedCompassData, 0, mCompassTimestamp);
                ALOGI_IF(INPUT_DATA,
                         "%s %+8ld %+8ld %+8ld  %lld", __func__,
                         mCachedCompassData[0], mCachedCompassData[1],
                         mCachedCompassData[2], mCompassTimestamp);
                mHasPendingEvent = true;
                n = 0; /* data not sent now but with mHasPendingEvent */
            } else {
// FIXME: Need to apply offset and scale to non-fusion-owned data with same FD
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        } else { /* sensors_meta_data_event_t */
            if (mPendingEvent.meta_data.sensor ==
                                        mPendingEvents[MagneticField].sensor) {
                mFlushPSensor |= (1 << MagneticField);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else {
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        }
    }
    return n;
}

int NvsInv::readEvents(sensors_event_t *data, int count, int32_t handle)
{
    VHANDLER_LOG;
    int n = 0;
    int ret;

    if (mFlush) {
        for (int i = 0; count && mFlush && i < MpuNumSensors; i++) {
            if (mFlush & (1 << i)) {
                /* if sensor i needs to send a flush event */
                mFlush &= ~(1 << i); /* clear flag */
                mMetaDataEvent.meta_data.sensor = mPendingEvents[i].sensor;
                mMetaDataEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
                *data++ = mMetaDataEvent;
                count--;
                n++;
                ALOGI_IF(SensorBase::INPUT_DATA || SensorBase::EXTRA_VERBOSE,
                         "%s sensors_meta_data_event_t version=%d type=%d\n"
                         "meta_data.what=%d meta_data.sensor=%d\n",
                         __func__,
                         mMetaDataEvent.version,
                         mMetaDataEvent.type,
                         mMetaDataEvent.meta_data.what,
                         mMetaDataEvent.meta_data.sensor);
            }
        }
    }
    if (!count)
        return n;

    if (handle > 0) {
        int snsrIndex = getHandleIndex(handle);
        if (snsrIndex < 0) {
            ALOGE("%s ERR handle=%d\n", __func__, handle);
            return -EINVAL;
        }

        switch (snsrIndex) {
        case MagneticField:
        case RawMagneticField:
            ret = buildCompassEvent(data, count);
            if (ret > 0)
                n += ret;
            break;

        default:
            ret = buildMpuEvent(data, count);
            if (ret > 0)
                n += ret;
            break;
        }

        if (mFlushPSensor) {
            /* if there was a flush event from a physical sensor */
            for (int i = 0; i < MpuNumSensors; i++) {
                /* test which sensors needed a physical sensor flush */
                if (mFlushSensor[i]) {
                    /* this sensor is awaiting physical sensor(s) flush */
                    if (mFlushSensor[i] & mFlushPSensor) {
                        /* this sensor needs this physical sensor flush */
                        mFlushSensor[i] &= ~mFlushPSensor; /* remove flag */
                        if (!mFlushSensor[i])
                            /* don't need anymore physical sensor flushes */
                            mFlush |= (1 << i); /* flag to send event */
                    }
                }
            }
            mFlushPSensor = 0;
        }
        return n;
    }

    mHasPendingEvent = false;
    inv_execute_on_data();

    int numEventReceived = 0;

    long msg;
    msg = inv_get_message_level_0(1);
    if (msg) {
        if (msg & INV_MSG_MOTION_EVENT) {
            ALOGI_IF(PROCESS_VERBOSE, "HAL:**** Motion ****\n");
        }
        if (msg & INV_MSG_NO_MOTION_EVENT) {
            ALOGI_IF(PROCESS_VERBOSE, "HAL:***** No Motion *****\n");
            /* after the first no motion, the gyro should be
               calibrated well */
            mGyroAccuracy = SENSOR_STATUS_ACCURACY_HIGH;
            /* if gyros are on and we got a no motion, set a flag
               indicating that the cal file can be written. */
            mHaveGoodMpuCal = true;
        }
        if(msg & INV_MSG_NEW_AB_EVENT) {
            ALOGI_IF(EXTRA_VERBOSE, "HAL:***** New Accel Bias *****\n");
            getAccelBias();
            mAccelAccuracy = inv_get_accel_accuracy();
        }
        if(msg & INV_MSG_NEW_GB_EVENT) {
            ALOGI_IF(EXTRA_VERBOSE, "HAL:***** New Gyro Bias *****\n");
            getGyroBias();
            setGyroBias();
        }
        if(msg & INV_MSG_NEW_FGB_EVENT) {
            ALOGI_IF(EXTRA_VERBOSE, "HAL:***** New Factory Gyro Bias *****\n");
            getFactoryGyroBias();
        }
        if(msg & INV_MSG_NEW_FAB_EVENT) {
            ALOGI_IF(EXTRA_VERBOSE, "HAL:***** New Factory Accel Bias *****\n");
            getFactoryAccelBias();
        }
        if(msg & INV_MSG_NEW_CB_EVENT) {
            ALOGI_IF(EXTRA_VERBOSE, "HAL:***** New Compass Bias *****\n");
            getCompassBias();
            mCompassAccuracy = inv_get_mag_accuracy();
        }
    }

    for (int i = 0; i < MpuNumSensors; i++) {
        int update = 0;

        // load up virtual sensors
        if (mEnabled & (1 << i)) {
            update = CALL_MEMBER_FN(this, mHandlers[i])(mPendingEvents + i);
            if (update && (count > 0)) {
                *data++ = mPendingEvents[i];
                count--;
                numEventReceived++;
            }
        }
    }

    return numEventReceived;
}

bool NvsInv::hasPendingEvents() {
    VFUNC_LOG;

    return mHasPendingEvent;
}

int NvsInv::getFd(int32_t handle)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d\n", __func__, handle);
    return -1;
}

#if 0
int NvsInv::inv_read_temperature(long long *data)
{
    VHANDLER_LOG;
    int64_t timestamp;
    long temp;
    int raw;
    int ret;

    if (mGyroTempSensor == NULL)
        return -1;

    ret = mGyroTempSensor->readRaw(-1, &raw);
    if (!ret) {
        float fVal;
        /* get current timestamp */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts) ;
        timestamp = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
        data[1] = timestamp;

        temp = raw;
        temp <<= 16;
        ret = mGyroTempSensor->getScale(-1, &fVal);
        if (!ret)
            temp *= fVal;
        ret = mGyroTempSensor->getOffset(-1, &fVal);
        if (!ret)
            temp += fVal;
        data[0] = temp;
        ALOGI_IF(ENG_VERBOSE && INPUT_DATA,
                 "%s raw = %d, temperature = %ld, timestamp = %lld",
                 __func__, raw, temp, timestamp);
    }
    return ret;
}
#endif //0

/* these functions can be consolidated
with inv_convert_to_body_with_scale */
void NvsInv::getCompassBias()
{
    VFUNC_LOG;

    long bias[3];
    long compassBias[3];
    unsigned short orient;
    signed char orientMtx[9];

    if (mCompassSensor == NULL)
        return;

    mCompassSensor->getMatrix(0, orientMtx);
    /* disable NVS matrix once fusion has it */
    mCompassSensor->setMatrix(0, matrixDisable);
    orient = inv_orientation_matrix_to_scalar(orientMtx);

    /* Get Values from MPL */
    inv_get_compass_bias(bias);
    inv_convert_to_body(orient, bias, compassBias);
    ALOGI_IF(HANDLER_DATA, "Mpl Compass Bias (HW unit) %ld %ld %ld",
             bias[0], bias[1], bias[2]);
    ALOGI_IF(HANDLER_DATA, "Mpl Compass Bias (HW unit) (body) %ld %ld %ld",
             compassBias[0], compassBias[1], compassBias[2]);
    long compassSensitivity = inv_get_compass_sensitivity();
    if (compassSensitivity == 0) {
        compassSensitivity = mCompassScale;
    }
    for (int i = 0; i < 3; i++) {
        /* convert to uT */
        float temp = (float) compassSensitivity / (1L << 30);
        mCompassBias[i] =(float) (compassBias[i] * temp / 65536.f);
    }

    return;
}

void NvsInv::getFactoryGyroBias()
{
    VFUNC_LOG;

    /* Get Values from MPL */
    inv_get_gyro_bias(mFactoryGyroBias);
    ALOGI_IF(ENG_VERBOSE, "Factory Gyro Bias %ld %ld %ld",
             mFactoryGyroBias[0], mFactoryGyroBias[1], mFactoryGyroBias[2]);
    mFactoryGyroBiasAvailable = true;

    return;
}

/* set bias from factory cal file to MPU offset (in chip frame)
   x = values store in cal file --> (v/1000 * 2^16 / (2000/250))
   offset = x/2^16 * (Gyro scale / self test scale used) * (-1) / offset scale
   i.e. self test default scale = 250
         gyro scale default to = 2000
         offset scale = 4 //as spec by hardware
         offset = x/2^16 * (8) * (-1) / (4)
*/
void NvsInv::setFactoryGyroBias()
{
    VFUNC_LOG;
    bool err = false;
    int ret;

    if ((mGyroSensor == NULL) || (mFactoryGyroBiasAvailable == false))
        return;

    /* add scaling here - depends on self test parameters */
    int scaleRatio = mGyroScale / mGyroSelfTestScale;
    int offsetScale = 4;
    float tempBias;

    ALOGI_IF(ENG_VERBOSE, "%s: scaleRatio used =%d", __func__, scaleRatio);
    ALOGI_IF(ENG_VERBOSE, "%s: offsetScale used =%d", __func__, offsetScale);

    for (int i = 0; i < 3; i++) {
        tempBias = (((float)mFactoryGyroBias[i]) / 65536.f * scaleRatio) * -1 / offsetScale;
        ret = mGyroSensor->setOffset(-1, tempBias, i);
        if (ret) {
            ALOGE("%s ERR: writing gyro offset %f to channel %u",
                  __func__, tempBias, i);
            err = true;
        } else {
            ALOGI_IF(EXTRA_VERBOSE, "%s channel %u = %f - (%lld)",
                     __func__, i, tempBias, getTimestamp());
        }
    }
    if (err)
        return;

    mFactoryGyroBiasAvailable = false;
    ALOGI_IF(EXTRA_VERBOSE, "%s: Factory gyro Calibrated Bias Applied",
             __func__);
}

/* these functions can be consolidated
with inv_convert_to_body_with_scale */
void NvsInv::getGyroBias()
{
    VFUNC_LOG;

    long *temp = NULL;
    long chipBias[3];
    long bias[3];
    unsigned short orient;

    /* Get Values from MPL */
    inv_get_mpl_gyro_bias(mGyroChipBias, temp);
    orient = inv_orientation_matrix_to_scalar(mGyroOrientation);
    inv_convert_to_body(orient, mGyroChipBias, bias);
    ALOGI_IF(ENG_VERBOSE, "Mpl Gyro Bias (HW unit) %ld %ld %ld",
             mGyroChipBias[0], mGyroChipBias[1], mGyroChipBias[2]);
    ALOGI_IF(ENG_VERBOSE, "Mpl Gyro Bias (HW unit) (body) %ld %ld %ld",
             bias[0], bias[1], bias[2]);
    long gyroSensitivity = inv_get_gyro_sensitivity();
    if(gyroSensitivity == 0) {
        gyroSensitivity = mGyroScale;
    }

    /* scale and convert to rad */
    for(int i=0; i<3; i++) {
        float temp = (float) gyroSensitivity / (1L << 30);
        mGyroBias[i] = (float) (bias[i] * temp / (1<<16) / 180 * M_PI);
        if (mGyroBias[i] != 0)
            mGyroBiasAvailable = true;
    }

    return;
}

void NvsInv::setGyroZeroBias()
{
    VFUNC_LOG;
    bool err = false;

}

void NvsInv::setGyroBias()
{
    VFUNC_LOG;

    if(mGyroBiasAvailable == false)
        return;

    long bias[3];
    long gyroSensitivity = inv_get_gyro_sensitivity();

    if(gyroSensitivity == 0) {
        gyroSensitivity = mGyroScale;
    }

    inv_get_gyro_bias_dmp_units(bias);

    return;
}

void NvsInv::getFactoryAccelBias()
{
    VFUNC_LOG;

    long temp;

    /* Get Values from MPL */
    inv_get_accel_bias(mFactoryAccelBias);
    ALOGI_IF(ENG_VERBOSE, "Factory Accel Bias (mg) %ld %ld %ld",
             mFactoryAccelBias[0], mFactoryAccelBias[1], mFactoryAccelBias[2]);
    mFactoryAccelBiasAvailable = true;

    return;
}

void NvsInv::setFactoryAccelBias()
{
    VFUNC_LOG;
    bool err = false;
    unsigned int i;
    int ret;

    if ((mAccelSensor == NULL) || (mFactoryAccelBiasAvailable == false))
        return;

    /* add scaling here - depends on self test parameters */
    int scaleRatio = mAccelScale / mAccelSelfTestScale;
    int offsetScale = 16;
    float tempBias;

    ALOGI_IF(ENG_VERBOSE, "%s: scaleRatio used =%d", __func__, scaleRatio);
    ALOGI_IF(ENG_VERBOSE, "%s: offsetScale used =%d", __func__, offsetScale);

    for (i = 0; i < 3; i++) {
        tempBias = -mFactoryAccelBias[i] / 65536.f * scaleRatio / offsetScale;
        ret = mAccelSensor->setOffset(-1, tempBias, i);
        if (ret) {
            ALOGE("%s ERR: writing accel offset channel %u", __func__, i);
            err = true;
        } else {
            ALOGI_IF(EXTRA_VERBOSE, "%s channel %u = %f - (%lld)",
                     __func__, i, tempBias, getTimestamp());
        }
    }
    if (err)
        return;

    mFactoryAccelBiasAvailable = false;
    ALOGI_IF(EXTRA_VERBOSE, "%s: Factory Accel Calibrated Bias Applied",
             __func__);
}

void NvsInv::getAccelBias()
{
    VFUNC_LOG;
    long temp;

    /* Get Values from MPL */
    inv_get_mpl_accel_bias(mAccelBias, &temp);
    ALOGI_IF(ENG_VERBOSE, "Accel Bias (mg) %ld %ld %ld",
             mAccelBias[0], mAccelBias[1], mAccelBias[2]);
    mAccelBiasAvailable = true;

    return;
}

/*    set accel bias obtained from MPL
      bias is scaled by 65536 from MPL
      DMP expects: bias * 536870912 / 2^30 = bias / 2 (in body frame)
*/
void NvsInv::setAccelBias()
{
    VFUNC_LOG;

    if(mAccelBiasAvailable == false) {
        ALOGI_IF(ENG_VERBOSE, "HAL: setAccelBias - accel bias not available");
        return;
    }

    return;
}

void NvsInv::initBias()
{
    VFUNC_LOG;
    int i;

    if (mAccelSensor != NULL) {
        for (i = 0; i < 3; i++)
            mAccelSensor->setOffset(-1, 0, i);
    }
    setGyroZeroBias();
    if (mGyroSensor != NULL) {
        for (i = 0; i < 3; i++)
            mGyroSensor->setOffset(-1, 0, i);
    }
    return;
}

void NvsInv::inv_calculate_bias(float *cal, float *raw, float *bias)
{
    if (!cal || !raw)
        return;

    bias[0] = raw[0] - cal[0];
    bias[1] = raw[1] - cal[1];
    bias[2] = raw[2] - cal[2];
    return ;
}

int NvsInv::isCompassDisabled(void)
{
    VFUNC_LOG;
    int fd;

    if (mCompassSensor == NULL)
        return 1;

    fd = mCompassSensor->getFd();
    if (fd < 0)
        return 1;

    return 0;
}

/*  these handlers transform mpl data into one of the Android sensor types */
int NvsInv::HandlerGyro(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_gyroscope(s->gyro.v, &s->gyro.status, &ts);
    s->timestamp = ts;
    s->gyro.status = mGyroAccuracy;
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->gyro.v[0], s->gyro.v[1], s->gyro.v[2],
             s->timestamp, s->gyro.status, update);
    return update;
}

int NvsInv::HandlerRawGyro(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_gyroscope_raw(s->uncalibrated_gyro.uncalib,
                                               &s->gyro.status, &ts);
    s->timestamp = ts;

    float cal_gyro[3];
    float bias[3];
    int8_t status;

    memset(bias, 0, sizeof(bias));
    inv_get_sensor_type_gyroscope(cal_gyro, &status, &ts);
    if (mGyroAccuracy != 0)
        inv_calculate_bias(cal_gyro, s->uncalibrated_gyro.uncalib, bias);

    if (update)
        memcpy(s->uncalibrated_gyro.bias, bias, sizeof(bias));
    s->gyro.status = SENSOR_STATUS_UNRELIABLE;
    ALOGI_IF(HANDLER_DATA,
             "%s %f %f %f %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->uncalibrated_gyro.uncalib[0],
             s->uncalibrated_gyro.uncalib[1],
             s->uncalibrated_gyro.uncalib[2],
             s->uncalibrated_gyro.bias[0],
             s->uncalibrated_gyro.bias[1],
             s->uncalibrated_gyro.bias[2],
             s->timestamp, s->gyro.status, update);
    return update;
}

int NvsInv::HandlerAccelerometer(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;
    update = inv_get_sensor_type_accelerometer(s->acceleration.v,
                                               &s->acceleration.status, &ts);
    s->timestamp = ts;
    s->acceleration.status = mAccelAccuracy;
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->acceleration.v[0], s->acceleration.v[1], s->acceleration.v[2],
             s->timestamp, s->acceleration.status, update);
    return update;
}

int NvsInv::HandlerMagneticField(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_magnetic_field(s->magnetic.v,
                                                &s->magnetic.status, &ts);
    s->timestamp = ts;
    s->magnetic.status = mCompassAccuracy;
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->magnetic.v[0], s->magnetic.v[1], s->magnetic.v[2],
             s->timestamp, s->magnetic.status, update);
    return update;
}

int NvsInv::HandlerRawMagneticField(sensors_event_t* s)
{
    VHANDLER_LOG;
    float bias[3];
    inv_time_t ts;
    int update;

    memset(bias, 0, sizeof(bias));
    //TODO: need to handle uncalib data and bias for 3rd party compass
    float cal_compass[3];
    int8_t status;

    update = inv_get_sensor_type_magnetic_field_raw(s->uncalibrated_magnetic.uncalib,
                                                    &s->magnetic.status, &ts);
    s->timestamp = ts;
    inv_get_sensor_type_magnetic_field(cal_compass, &status, &ts);
    if (mCompassAccuracy != 0)
        inv_calculate_bias(cal_compass,
                           s->uncalibrated_magnetic.uncalib, bias);
    if(update)
        memcpy(s->uncalibrated_magnetic.bias, bias, sizeof(bias));
    s->magnetic.status = SENSOR_STATUS_UNRELIABLE;
    ALOGI_IF(HANDLER_DATA,
             "%s %f %f %f %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->uncalibrated_magnetic.uncalib[0],
             s->uncalibrated_magnetic.uncalib[1],
             s->uncalibrated_magnetic.uncalib[2],
             s->uncalibrated_magnetic.bias[0],
             s->uncalibrated_magnetic.bias[1],
             s->uncalibrated_magnetic.bias[2],
             s->timestamp, s->magnetic.status, update);
    return update;
}

int NvsInv::HandlerOrientation(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_orientation(s->orientation.v,
                                             &s->orientation.status, &ts);
    s->timestamp = ts;
#ifdef BIAS_CONFIDENCE_HIGH_3
    s->orientation.status =
               mCompassBiasApplied ? BIAS_CONFIDENCE_HIGH_3 : mCompassAccuracy;
#else
    s->orientation.status = mCompassAccuracy;
#endif
    update |= isCompassDisabled();
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->orientation.v[0], s->orientation.v[1], s->orientation.v[2],
             s->timestamp, s->orientation.status, update);
    return update;
}

int NvsInv::HandlerRotationVector(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int8_t status;
    int update = 1;

    inv_get_sensor_type_rotation_vector(s->data, &status, &ts);
    s->timestamp = ts;
    s->orientation.status = mCompassAccuracy;
    update |= isCompassDisabled();
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->data[0], s->data[1], s->data[2], s->data[3], s->data[4],
             s->timestamp, s->orientation.status, update);
    return update;
}

int NvsInv::HandlerGameRotationVector(sensors_event_t* s)
{
    VHANDLER_LOG;
    int8_t status;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_rotation_vector_6_axis(s->data, &status, &ts);
    s->timestamp = ts;
    s->orientation.status = mAccelAccuracy;
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f %f %f  %lld ret=%d\n", __func__,
             s->data[0], s->data[1], s->data[2], s->data[3], s->data[4],
             s->timestamp, update);
    return update;
}

int NvsInv::HandlerLinearAccel(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_linear_acceleration(s->gyro.v,
                                                     &s->gyro.status, &ts);
    s->timestamp = ts;
    s->gyro.status = mAccelAccuracy;
    update |= isCompassDisabled();
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->gyro.v[0], s->gyro.v[1], s->gyro.v[2],
             s->timestamp, s->gyro.status, update);
    return update;
}

int NvsInv::HandlerGravity(sensors_event_t* s)
{
    VHANDLER_LOG;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_gravity(s->gyro.v, &s->gyro.status, &ts);
    s->timestamp = ts;
    s->gyro.status = mCompassAccuracy;
    update |= isCompassDisabled();
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->gyro.v[0], s->gyro.v[1], s->gyro.v[2],
             s->timestamp, s->gyro.status, update);
    return update;
}

int NvsInv::HandlerGeomagneticRotationVector(sensors_event_t* s)
{
    VHANDLER_LOG;
    int8_t status;
    inv_time_t ts;
    int update;

    update = inv_get_sensor_type_geomagnetic_rotation_vector(s->data,
                                                             &status, &ts);
    s->timestamp = ts;
    s->orientation.status = mCompassAccuracy;
    ALOGI_IF(HANDLER_DATA, "%s %f %f %f %f %f  %lld sts=%d ret=%d\n", __func__,
             s->data[0], s->data[1], s->data[2], s->data[3], s->data[4],
             s->timestamp, s->orientation.status, update);
    return update < 1 ? 0 :1;
}
