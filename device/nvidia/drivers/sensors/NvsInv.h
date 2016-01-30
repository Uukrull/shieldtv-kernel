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

#ifndef NVS_INVENSENSE_H
#define NVS_INVENSENSE_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <poll.h>
#include <time.h>

#include "SensorBase.h"
#include "MPLSensorDefs.h"

#include "NvsAccel.h"
#include "NvsAnglvel.h"
#include "NvsMagn.h"
#include "NvsTemp.h"

/*****************************************************************************/
/* Sensors Enable/Disable Mask
 *****************************************************************************/
#define MAX_CHIP_ID_LEN             (20)

#define INV_THREE_AXIS_GYRO         (0x000F)
#define INV_THREE_AXIS_ACCEL        (0x0070)
#define INV_THREE_AXIS_COMPASS      (0x0380)
#define INV_ALL_SENSORS             (0x7FFF)

#ifdef INVENSENSE_COMPASS_CAL
#define ALL_MPL_SENSORS_NP          (INV_THREE_AXIS_ACCEL \
                                      | INV_THREE_AXIS_COMPASS \
                                      | INV_THREE_AXIS_GYRO)
#else
#define ALL_MPL_SENSORS_NP          (INV_THREE_AXIS_ACCEL \
                                      | INV_THREE_AXIS_COMPASS \
                                      | INV_THREE_AXIS_GYRO)
#endif

#define PHYSICAL_SENSOR_MASK            ((1 << Accelerometer) | \
                                         (1 << Gyro) | \
                                         (1 << MagneticField))
// mask of virtual sensors that require gyro + accel + compass data
#define VIRTUAL_SENSOR_9AXES_MASK (         \
        (1 << Orientation)                  \
        | (1 << RotationVector)             \
        | (1 << LinearAccel)                \
        | (1 << Gravity)                    \
)
// mask of virtual sensors that require gyro + accel data (but no compass data)
#define VIRTUAL_SENSOR_GYRO_6AXES_MASK (    \
        (1 << GameRotationVector)           \
)
// mask of virtual sensors that require mag + accel data (but no gyro data)
#define VIRTUAL_SENSOR_MAG_6AXES_MASK (     \
        (1 << GeomagneticRotationVector)    \
)
// mask of all virtual sensors
#define VIRTUAL_SENSOR_ALL_MASK (           \
        VIRTUAL_SENSOR_9AXES_MASK           \
        | VIRTUAL_SENSOR_GYRO_6AXES_MASK    \
        | VIRTUAL_SENSOR_MAG_6AXES_MASK     \
)

// bit mask of current MPL active features (mMplFeatureActiveMask)
#define INV_COMPASS_CAL              0x01
#define INV_COMPASS_FIT              0x02

/* conversion of sensor rates */
#define DEFAULT_MPL_GYRO_RATE           (20000L)     //us
#define DEFAULT_MPL_COMPASS_RATE        (20000L)     //us


class NvsInv: public SensorBase
{
    typedef int (NvsInv::*hfunc_t)(sensors_event_t*);

public:

    NvsInv(NvsAccel *,
              NvsAnglvel *,
              NvsTemp *,
              NvsMagn *);
    virtual ~NvsInv();

    virtual int setDelay(int32_t handle, int64_t ns, int channel = -1);
    virtual int enable(int32_t handle, int enable, int channel = -1);
    virtual int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
    virtual int flush(int handle);
    virtual int readEvents(sensors_event_t *data, int count, int32_t handle = -1);
    virtual int getFd(int32_t handle = -1);
    virtual bool hasPendingEvents();
    int getSensorList(struct sensor_t *list, int index, int limit);
    int buildCompassEvent(sensors_event_t *data, int count);
    int buildMpuEvent(sensors_event_t *data, int count);
    int mpuEvent(sensors_event_t *data, int count, Nvs *sensor);

protected:
    NvsAccel *mAccelSensor;
    NvsAnglvel *mGyroSensor;
    NvsTemp *mGyroTempSensor;
    NvsMagn *mCompassSensor;
    struct sensor_t *mSensorList[MpuNumSensors];
    uint32_t mFlushSensor[MpuNumSensors];
    uint32_t mFlushPSensor;
    uint32_t mFlush;

    int HandlerGyro(sensors_event_t *data);
    int HandlerRawGyro(sensors_event_t *data);
    int HandlerAccelerometer(sensors_event_t *data);
    int HandlerMagneticField(sensors_event_t *data);
    int HandlerRawMagneticField(sensors_event_t *data);
    int HandlerOrientation(sensors_event_t *data);
    int HandlerRotationVector(sensors_event_t *data);
    int HandlerGameRotationVector(sensors_event_t *data);
    int HandlerLinearAccel(sensors_event_t *data);
    int HandlerGravity(sensors_event_t *data);
    int HandlerGeomagneticRotationVector(sensors_event_t *data);

    void inv_set_device_properties();
    int inv_constructor_init();
    int inv_constructor_default_enable();

    bool mHaveGoodMpuCal;   // flag indicating that the cal file can be written
    int mGyroAccuracy;      // value indicating the quality of the gyro calibr.
    int mAccelAccuracy;     // value indicating the quality of the accel calibr.
    int mCompassAccuracy;   // value indicating the quality of the compass calibr.

    uint32_t mEnabled;
    sensors_meta_data_event_t mMetaDataEvent;
    sensors_event_t mPendingEvents[MpuNumSensors];
    sensors_event_t mPendingEvent;
    int64_t mSamplePeriodHw[MpuNumSensors];
    int64_t mSamplePeriodNs[MpuNumSensors];
    int64_t mMaxReportLatencyNs[MpuNumSensors];
    hfunc_t mHandlers[MpuNumSensors];
    short mCachedGyroData[3];
    long mCachedAccelData[3];
    long mCachedCompassData[3];

    bool mHasPendingEvent;
    int mAccelScale;
    long mAccelSelfTestScale;
    long mGyroScale;
    long mGyroSelfTestScale;
    long mCompassScale;
    float mCompassBias[3];
    bool mFactoryGyroBiasAvailable;
    long mFactoryGyroBias[3];
    bool mGyroBiasAvailable;
    bool mGyroBiasApplied;
    float mGyroBias[3];    //in body frame
    long mGyroChipBias[3]; //in chip frame
    bool mFactoryAccelBiasAvailable;
    long mFactoryAccelBias[3];
    bool mAccelBiasAvailable;
    bool mAccelBiasApplied;
    long mAccelBias[3];    //in chip frame

    char chip_ID[MAX_CHIP_ID_LEN];

    signed char mGyroOrientation[9];
    signed char mAccelOrientation[9];

    int64_t mCompassTimestamp;

private:
    int getHandleIndex(int32_t handle);
    unsigned int getPhysSnsrMask(int snsrIndex);
    void storeCalibration();
    void getCompassBias();
    void getFactoryGyroBias();
    void setFactoryGyroBias();
    void getGyroBias();
    void setGyroZeroBias();
    void setGyroBias();
    void getFactoryAccelBias();
    void setFactoryAccelBias();
    void getAccelBias();
    void setAccelBias();
    int isCompassDisabled();
    void initBias();
    void inv_calculate_bias(float *cal, float *raw, float *bias);
};

extern "C" {
    void setCallbackObject(NvsInv*);
    NvsInv *getCallbackObject();
}

#endif  // NVS_INVENSENSE_H
