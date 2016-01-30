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

/* The NVS = NVidia Sensor framework */
/* The NVS implementation of populating the sensor list is
 * provided by the kernel driver with the following sysfs attributes:
 * - part = struct sensor_t.name
 * - vendor = struct sensor_t.vendor
 * - version = struct sensor_t.version
 * - in_<iio_device>_peak_raw = struct sensor_t.maxRange
 * - in_<iio_device>_peak_scale = struct sensor_t.resolution
 * - milliamp = struct sensor_t.power
 * - in_<iio_device>_batch_period = struct sensor_t.minDelay
 *                                  (read when device is off)
 * - in_<iio_device>_batch_timeout = struct sensor_t.maxDelay
 *                                   (read when device is off)
 * - fifo_reserved_event_count = struct sensor_t.fifoReservedEventCount
 * - fifo_max_event_count = struct sensor_t.fifoMaxEventCount
 * The struct sensor_t.minDelay is obtained by reading
 * in_<iio_device>_batch_period when the device is off.
 * The struct sensor_t.maxDelay is obtained by reading
 * in_<iio_device>_batch_timeout when the device is off.
 * Attributes prefaced with in_ are actual IIO attributes that NVS uses.
 */
/* The NVS implementation of batch and flush are as follows:
 * The kernel driver must have the following IIO sysfs attributes:
 * - in_<iio_device>_batch_period
 *       read: if device enabled then returns the last _batch_period written.
 *             if device disabled then returns minDelay.
 *       write: sets the batch_period parameter.
 * - in_<iio_device>_batch_timeout
 *       read: if device enabled then returns the last _batch_timeout written.
 *             if device disabled then returns maxDelay.
 *       write: sets the batch_timeout and initiates the batch command using the
 *          batch_period parameter.
 * - in_<iio_device>_flush
 *       read: returns the batch_flags that are supported.  If batch/flush is
 *             supported, then bit 0 must be 1.
 *       write: initiates a flush.
 * When the batch is done, the following sysfs attribute write order is required
 * by the NVS kernel driver:
 * 1. _batch_flags
 * 2. _batch_timeout
 * 3. _batch_period
 * The write to the _batch_period sysfs attribute is what initiates a batch
 * command and uses the last _batch_flags and _batch_timeout parameters
 * written.
 * When a flush is initiated, NVS writes to the sysfs in_<iio_device>_flush
 * attribute and sets the mFlush flag.  When the flush is completed, the kernel
 * driver will send a timestamp = 0.  When NVS detects timestamp = 0 along with
 * the mFlush flag, the flag is cleared and the sensors_meta_data_event_t is
 * sent.
 */
/* NVS requires the buffer timestamp to be supported and enabled. */
/* The NVS HAL will use the IIO scale and offset sysfs attributes to modify the
 * data using the following formula: (data * scale) + offset
 * A scale value of 0 disables scale.
 * A scale value of 1 puts the NVS HAL into calibration mode where the scale
 * and offset are read everytime the data is read to allow realtime calibration
 * of the scale and offset values to be used in the device tree parameters.
 * Keep in mind the data is buffered but the NVS HAL will display the data and
 * scale/offset parameters in the log.  See calibration steps below.
 */
/* NVS has a calibration mechanism:
 * 1. Disable device.
 * 2. Write 1 to the scale sysfs attribute.
 * 3. Enable device.
 * 4. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 5. Write to scale and offset sysfs attributes as needed to get the data
 *    modified as desired.
 * 6. Disabling the device disables calibration mode.
 * 7. Set the new scale and offset parameters in the device tree:
 *    <IIO channel>_scale_val
 *    <IIO channel>_scale_val2
 *    <IIO channel>_offset_val
 *    <IIO channel>_offset_val2
 *    The values are in IIO IIO_VAL_INT_PLUS_MICRO format.
 */
/* NVS kernel drivers have a test mechanism for sending specific data up the
 * SW stack by writing the requested data value to the IIO raw sysfs attribute.
 * That data will be sent anytime there would normally be new data from the
 * device.
 * The feature is disabled whenever the device is disabled.  It remains
 * disabled until the IIO raw sysfs attribute is written to again.
 */


#include <fcntl.h>
#include <cutils/log.h>
#include <stdlib.h>
#include "NvsDev.h"
#include "NvsIio.h"
#include "Nvs.h"


Nvs::Nvs(int devNum,
         const char *name,
         int type,
         enum NVS_DRIVER_TYPE driverType,
         Nvs *link)
    : SensorBase(NULL, NULL),
      mName(name),
      mType(type),
      mLink(link),
      mNvsDev(NULL),
      mNvsCh(NULL),
      mSensorList(NULL),
      mEnable(0),
      mNvsChN(0),
      mVs(false),
      mFlush(false),
      mHasPendingEvent(false),
      mFirstEvent(false),
      mCalibrationMode(false),
      mMatrixEn(false)
{
    FILE *fp;
    char str[NVS_SYSFS_PATH_SIZE_MAX];
    int i;
    int iVal;
    int ret;

    memset(&mMatrix, 0, sizeof(mMatrix));
    /* init event structures */
    memset(&mEvent, 0, sizeof(mEvent));
    mEvent.version = sizeof(sensors_event_t);
    mEvent.type = type;
    memset(&mMetaDataEvent, 0, sizeof(mMetaDataEvent));
    mMetaDataEvent.version = META_DATA_VERSION;
    mMetaDataEvent.type = SENSOR_TYPE_META_DATA;
    if (mLink == NULL) {
        /* we only need one instance of the device interface */
        switch (driverType) {
        case NVS_DRIVER_TYPE_IIO:
            mNvsDev = new NvsIio(this, devNum);
            break;

#if 0 // Add other driver type support here
        case NVS_DRIVER_TYPE_INPUT:
            mNvsDev = new NvsInput(this, devNum);
            break;

        case NVS_DRIVER_TYPE_RELAY:
            mNvsDev = new NvsRelay(this, devNum);
            break;
#endif // 0

        default:
            ALOGE("%s ERR: unsupported NVS_DRIVER_TYPE_\n", __func__);
            goto errExit;
        }
    } else {
        /* the derived classes get directed to the one instance */
        mNvsDev = mLink->mNvsDev;
        if (mNvsDev != NULL)
            mNvsDev->devSetLinkRoot(this);
    }
    if (mNvsDev == NULL) {
        ALOGE("%s ERR: mNvsDev == NULL\n", __func__);
        goto errExit;
    }

    /* test if this is a virtual/single device */
    sprintf(str, "%s/nvs", mNvsDev->devSysfsPath());
    fp = fopen(str, "r");
    if (fp != NULL) {
        mVs = true;
        fclose(fp);
    }
    /* get the number of channels for this device */
    if (mVs)
        mNvsChN = mNvsDev->devGetChanN(NULL);
    else
        mNvsChN = mNvsDev->devGetChanN(mName);
    if (mNvsChN <= 0) {
        ALOGE("%s ERR: devGetChanN(%s)\n", __func__, mName);
        goto errExit;
    }

    /* allocate channel info pointer(s) memory */
    mNvsCh = new devChannel* [mNvsChN];
    /* get channel info */
    if (mVs)
        ret = mNvsDev->devGetChanInfo(NULL, mNvsCh);
    else
        ret = mNvsDev->devGetChanInfo(mName, mNvsCh);
    if (ret != mNvsChN) {
        ALOGE("%s ERR: devGetChanInfo(%s)\n", __func__, mName);
        goto errExit;
    }

    /* (optional) device orientation matrix on platform */
    readMatrix();
    matrixValidate();
    return;

errExit:
    NvsRemove();
}

Nvs::~Nvs()
{
    NvsRemove();
}

void Nvs::NvsRemove()
{
    if (mLink == NULL)
        delete mNvsDev;
    mNvsDev = NULL;
    delete mNvsCh;
    mNvsChN = 0;
}

Nvs *Nvs::getLink(void)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    return mLink;
}

int Nvs::getFd(int32_t handle)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d\n", __func__, handle);
    return mNvsDev->devFd(handle);
}

int Nvs::initFd(struct pollfd *pollFd, int32_t handle)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s pollFd=%p handle=%d\n", __func__, pollFd, handle);
    return mNvsDev->devFdInit(pollFd, handle);
}

int Nvs::enable(int32_t handle, int enable, int channel)
{
    float scale;
    int i;
    int ret;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d enable=%d channel=%d\n",
             __func__, handle, enable, channel);
    if (mNvsDev == NULL)
        return -EINVAL;

    if (handle < 1)
        handle = mEvent.sensor;

    ret = mNvsDev->devChanAble(handle, channel, enable);
    if (ret) {
        ALOGE("%s: %d -> handle %d ERR\n", __func__, enable, handle);
    } else {
        mEnable = enable;
        if (enable) {
            mFirstEvent = true;
            /* If scale == 1 then we're in calibration mode for the device.
             * This is where scale and offset are read on every sample and
             * applied to the data.  Calibration mode is disabled when the
             * device is disabled.
             */
            ret = sysfsFloatRead(mNvsCh[0]->attrPath[ATTR_SCALE],
                                 &scale);
            if ((scale == 1.0f) && !ret) {
                mCalibrationMode = true;
                ALOGI("%s handle=%d channel=%d calibration mode ENABLED\n",
                      __func__, handle, channel);
            }
        } else {
            if (mCalibrationMode) {
                /* read new scale and offset after calibration mode */
                for (i = 0; i < mNvsChN; i++) {
                    sysfsFloatRead(mNvsCh[i]->attrPath[ATTR_SCALE],
                                   &mNvsCh[i]->attrFVal[ATTR_SCALE]);
                    sysfsFloatRead(mNvsCh[i]->attrPath[ATTR_OFFSET],
                                   &mNvsCh[i]->attrFVal[ATTR_OFFSET]);
                }
                mCalibrationMode = false;
                ALOGI("%s handle=%d channel=%d calibration mode DISABLED\n",
                      __func__, handle, channel);
            }
        }
    }
    ALOGI_IF(SensorBase::PROCESS_VERBOSE,
             "%s handle=%d channel=%d enable=%d return=%d\n",
             __func__, handle, channel, enable, ret);
    return ret;
}

int Nvs::getEnable(int32_t handle, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d channel=%d\n", __func__, handle, channel);
    if (channel >= mNvsChN)
        return -EINVAL;

    if (channel < 0)
        channel = 0;

    return mNvsCh[channel]->enabled;
}

int Nvs::setAttr(int32_t handle, int channel, enum NVSB_ATTR attr, float fVal)
{
    int i;
    int limit;
    int ret = -EINVAL;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d channel=%d attr=%d fVal=%f\n",
             __func__, handle, channel, attr, fVal);
    if ((mNvsCh == NULL) || (channel >= mNvsChN))
        return -EINVAL;

    if ((channel < 0) && !mNvsCh[0]->attrShared[attr]) {
        /* write all channels */
        i = 0;
        limit = mNvsChN;
    } else if ((channel < 0) || mNvsCh[0]->attrShared[attr]) {
        /* write just first channel */
        i = 0;
        limit = 1;
    } else {
        /* write specific channel */
        i = channel;
        limit = channel + 1;
    }
    for (; i < limit; i++) {
        ret = sysfsFloatWrite(mNvsCh[i]->attrPath[attr], fVal);
        if (!ret) {
            ret = sysfsFloatRead(mNvsCh[i]->attrPath[attr],
                                 &mNvsCh[i]->attrFVal[attr]);
            if (ret) {
                mNvsCh[i]->attrCached[attr] = false;
                ret = 0; /* no error writing */
            } else {
                mNvsCh[i]->attrCached[attr] = true;
            }
        } else if (ret == -EINVAL) {
            /* -EINVAL == no path so store request and cache */
            mNvsCh[i]->attrFVal[attr] = fVal;
            mNvsCh[i]->attrCached[attr] = true;
        } else {
            ALOGE("%s: %f -> %s ERR: %d\n",
                  __func__, fVal, mNvsCh[i]->attrPath[attr], ret);
        }
    }
    return ret;
}

int Nvs::getAttr(int32_t handle, int channel, enum NVSB_ATTR attr, float *fVal)
{
    int ret = 0;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d channel=%d attr=%d fVal=%p\n",
             __func__, handle, channel, attr, fVal);
    if ((mNvsCh == NULL) || (channel >= mNvsChN))
        return -EINVAL;

    if (channel < 0)
        channel = 0;
    if (!mNvsCh[channel]->attrCached[attr]) {
        ret = sysfsFloatRead(mNvsCh[channel]->attrPath[attr],
                             &mNvsCh[channel]->attrFVal[attr]);
        if (!ret)
            mNvsCh[channel]->attrCached[attr] = true;
    }
    *fVal = mNvsCh[channel]->attrFVal[attr];
    return ret;
}

int Nvs::setDelay(int32_t handle, int64_t ns, int channel)
{
    enum NVSB_ATTR attr;
    float us = (float)(ns / 1000LL);

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d ns=%lld channel=%d\n",
             __func__, handle, ns, channel);
    if (mNvsCh[0]->attrPath[ATTR_DELAY])
        attr = ATTR_DELAY;
    else
        attr = ATTR_BATCH_PERIOD;
    return setAttr(handle, channel, attr, us);
}

int64_t Nvs::getDelay(int32_t handle, int channel)
{
    enum NVSB_ATTR attr;
    float us;
    int ret;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d channel=%d\n", __func__, handle, channel);
    if (mNvsCh[0]->attrPath[ATTR_DELAY])
        attr = ATTR_DELAY;
    else
        attr = ATTR_BATCH_PERIOD;
    ret = getAttr(handle, channel, attr, &us);
    if (ret)
        return ret;

    return (int64_t)us * 1000LL;
}

int Nvs::setOffset(int32_t handle, float offset, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d offset=%f\n",
             __func__, handle, channel, offset);
    int ret = setAttr(handle, channel, ATTR_OFFSET, offset);
    if (!ret)
        setSensorList();
    return ret;
}

int Nvs::getOffset(int32_t handle, float *offset, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d offset=%p\n",
             __func__, handle, channel, offset);
    return getAttr(handle, channel, ATTR_OFFSET, offset);
}

int Nvs::setMaxRange(int32_t handle, float maxRange, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d maxRange=%f\n",
             __func__, handle, channel, maxRange);
    int ret = setAttr(handle, channel, ATTR_MAX_RANGE, maxRange);
    if (!ret)
        setSensorList();
    return ret;
}

int Nvs::getMaxRange(int32_t handle, float *maxRange, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d maxRange=%p\n",
             __func__, handle, channel, maxRange);
    return getAttr(handle, channel, ATTR_MAX_RANGE, maxRange);
}

int Nvs::setResolution(int32_t handle, float resolution, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d resolution=%f\n",
             __func__, handle, channel, resolution);
    int ret = setAttr(handle, channel, ATTR_RESOLUTION, resolution);
    if (!ret)
        setSensorList();
    return ret;
}

int Nvs::getResolution(int32_t handle, float *resolution, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d resolution=%p\n",
             __func__, handle, channel, resolution);
    return getAttr(handle, channel, ATTR_RESOLUTION, resolution);
}

int Nvs::setScale(int32_t handle, float scale, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d scale=%f\n",
             __func__, handle, channel, scale);
    int ret = setAttr(handle, channel, ATTR_SCALE, scale);
    if (!ret)
        setSensorList();
    return ret;
}

int Nvs::getScale(int32_t handle, float *scale, int channel)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d channel=%d scale=%p\n",
             __func__, handle, channel, scale);
    return getAttr(handle, channel, ATTR_SCALE, scale);
}

int Nvs::batch(int handle, int flags,
               int64_t sampling_period_ns, int64_t max_report_latency_ns)
{
    int ret;
    float period_us = (float)(sampling_period_ns / 1000LL);
    float timeout_us = (float)max_report_latency_ns / 1000LL;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d flags=%d "
             "sampling_period_ns=%lld max_report_latency_ns=%lld\n", __func__,
             handle, flags, sampling_period_ns, max_report_latency_ns);
    /* flags is deprecated so commented out:
     * setAttr(handle, channel, ATTR_BATCH_FLAGS, flags);
     */
    /* It's possible the ATTR_BATCH_TIMEOUT attribute is not supported
     * if there is no HW FIFO.  Then only ATTR_BATCH_PERIOD is needed
     * and ATTR_BATCH_TIMEOUT is always assumed 0 in the kernel.
     */
    ret = setAttr(handle, -1, ATTR_BATCH_TIMEOUT, timeout_us);
    if (ret && max_report_latency_ns) {
        ALOGE("%s ERR: fifoMaxEventCount=%u handle=%d flags=%d "
              "sampling_period_ns=%lld max_report_latency_ns=%lld return=%d\n",
              __func__, mSensorList->fifoMaxEventCount,
              handle, flags, sampling_period_ns, max_report_latency_ns, ret);
        ret = -EINVAL;
    } else {
        enum NVSB_ATTR attr;

        if (mNvsCh[0]->attrPath[ATTR_BATCH_PERIOD])
            attr = ATTR_BATCH_PERIOD;
        else
            attr = ATTR_DELAY;
        ret = setAttr(handle, -1, attr, period_us);
        if (ret)
            ret = -EINVAL;
    }
    ALOGI_IF(SensorBase::PROCESS_VERBOSE,
             "%s fifoMaxEventCount=%u handle=%d flags=%d "
             "sampling_period_ns=%lld max_report_latency_ns=%lld return=%d\n",
             __func__, mSensorList->fifoMaxEventCount,
             handle, flags, sampling_period_ns, max_report_latency_ns, ret);
    return ret;
}

int Nvs::flush(int32_t handle)
{
    int ret = -EINVAL;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d\n", __func__, handle);
    if (mEnable) {
        ret = setAttr(handle, -1, ATTR_FLUSH, NVS_FLUSH_CMD_FLUSH);
        if (ret == -EINVAL)
            ret = 0;
        mFlush = true;
    }
    ALOGI_IF(SensorBase::PROCESS_VERBOSE,
             "%s handle=%d mFlush=%x return=%d\n",
             __func__, handle, mFlush, ret);
    return ret;
}

static const char *nvsStringType[] = {
    NULL,
    SENSOR_STRING_TYPE_ACCELEROMETER,
    SENSOR_STRING_TYPE_MAGNETIC_FIELD,
    SENSOR_STRING_TYPE_ORIENTATION,
    SENSOR_STRING_TYPE_GYROSCOPE,
    SENSOR_STRING_TYPE_LIGHT,
    SENSOR_STRING_TYPE_PRESSURE,
    SENSOR_STRING_TYPE_TEMPERATURE,
    SENSOR_STRING_TYPE_PROXIMITY,
    SENSOR_STRING_TYPE_GRAVITY,
    SENSOR_STRING_TYPE_LINEAR_ACCELERATION,
    SENSOR_STRING_TYPE_ROTATION_VECTOR,
    SENSOR_STRING_TYPE_RELATIVE_HUMIDITY,
    SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE,
    SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
    SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR,
    SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED,
    SENSOR_STRING_TYPE_SIGNIFICANT_MOTION,
    SENSOR_STRING_TYPE_STEP_DETECTOR,
    SENSOR_STRING_TYPE_STEP_COUNTER,
    SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
    SENSOR_STRING_TYPE_HEART_RATE,
    SENSOR_STRING_TYPE_TILT_DETECTOR,
    SENSOR_STRING_TYPE_WAKE_GESTURE,
    SENSOR_STRING_TYPE_GLANCE_GESTURE,
    SENSOR_STRING_TYPE_PICK_UP_GESTURE,
};

void Nvs::setSensorList()
{
    char path[NVS_SYSFS_PATH_SIZE_MAX];
    int en;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    if (mSensorList) {
        en = mEnable;
        if (mEnable)
            /* Need to turn off device to get some NVS attributes */
            enable(mEvent.sensor, 0);
        getMaxRange(mEvent.sensor, &mSensorList->maxRange);
        getResolution(mEvent.sensor, &mSensorList->resolution);
        if (mVs)
            sprintf(path, "%s/milliamp", mNvsDev->devSysfsPath());
        else
            sprintf(path, "%s/%s_milliamp", mNvsDev->devSysfsPath(), mName);
        sysfsFloatRead(path, &mSensorList->power);
        sysfsIntRead(mNvsCh[0]->attrPath[ATTR_BATCH_PERIOD],
                     &mSensorList->minDelay);
        if (mVs)
            sprintf(path, "%s/fifo_reserved_event_count",
                    mNvsDev->devSysfsPath());
        else
            sprintf(path, "%s/%s_fifo_reserved_event_count",
                    mNvsDev->devSysfsPath(), mName);
        sysfsIntRead(path, (int *)&mSensorList->fifoReservedEventCount);
        if (mVs)
            sprintf(path, "%s/fifo_max_event_count", mNvsDev->devSysfsPath());
        else
            sprintf(path, "%s/%s_fifo_max_event_count",
                    mNvsDev->devSysfsPath(), mName);
        sysfsIntRead(path, (int *)&mSensorList->fifoMaxEventCount);
        /* SENSORS_DEVICE_API_VERSION_1_3 */
        mSensorList->maxDelay = 0;
        sysfsIntRead(mNvsCh[0]->attrPath[ATTR_BATCH_TIMEOUT],
                     (int *)&mSensorList->maxDelay);
        if (en)
            enable(mEvent.sensor, en);
        if (SensorBase::ENG_VERBOSE) {
            ALOGI("--- %s sensor_t ---\n", mName);
            ALOGI("sensor_t.name=%s", mSensorList->name);
            ALOGI("sensor_t.vendor=%s", mSensorList->vendor);
            ALOGI("sensor_t.version=%d\n", mSensorList->version);
            ALOGI("sensor_t.handle=%d\n", mSensorList->handle);
            ALOGI("sensor_t.type=%d\n", mSensorList->type);
            ALOGI("sensor_t.maxRange=%f\n", mSensorList->maxRange);
            ALOGI("sensor_t.resolution=%f\n", mSensorList->resolution);
            ALOGI("sensor_t.power=%f\n", mSensorList->power);
            ALOGI("sensor_t.minDelay=%d\n", mSensorList->minDelay);
            ALOGI("sensor_t.fifoReservedEventCount=%d\n",
                  mSensorList->fifoReservedEventCount);
            ALOGI("sensor_t.fifoMaxEventCount=%d\n",
                  mSensorList->fifoMaxEventCount);
            /* SENSORS_DEVICE_API_VERSION_1_3 */
            ALOGI("sensor_t.stringType=%s\n", mSensorList->stringType);
            ALOGI("sensor_t.requiredPermission=%s\n",
                  mSensorList->requiredPermission);
            ALOGI("sensor_t.maxDelay=%d\n", (int)mSensorList->maxDelay);
            ALOGI("sensor_t.flags=%u\n", (unsigned int)mSensorList->flags);
        }
    }
}

int Nvs::getSensorList(struct sensor_t *list, int index, int limit)
{
    char path[NVS_SYSFS_PATH_SIZE_MAX];
    int i;
    int handle;
    int ret;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s list=%p index=%d limit=%d\n", __func__, list, index, limit);
    if (list == NULL) {
        if (mNvsChN)
            return 1; /* just return count if NULL */

        return 0; /* flag no sensor due to error */
    }

    if (limit < 1) /* return 0 if no rooom in list */
        return 0;

    if (mNvsChN <= 0)
        return 0; /* don't put sensor in list due to error */

    /* Remember our sensor in the global sensors list.
     * This is used to determine the sensor trigger mode as well as allowing us
     * to modify sensor list data at run-time, e.g. range/resolution changes.
     */
    mSensorList = &list[index];
    /* give handle to everyone that needs it */
    handle = index + 1;
    mEvent.sensor = handle;
    mMetaDataEvent.meta_data.sensor = handle;
    for (i = 0; i < mNvsChN; i++)
        mNvsCh[i]->handle = handle;
    mSensorList->handle = handle;
    if (mVs)
        sprintf(path, "%s/part", mNvsDev->devSysfsPath());
    else
        sprintf(path, "%s/%s_part", mNvsDev->devSysfsPath(), mName);
    ret = sysfsStrRead(path, mPart);
    if (ret <= 0) {
        /* if the NVS IIO extension for part is not supported
         * then use the driver name
         */
        sprintf(path, "%s/name", mNvsDev->devSysfsPath());
        sysfsStrRead(path, mPart);
    }
    /* Due to below "spec" in sensors.h and the possibility of multiple parts
     * with same name and type, we tack on the handle number to differentiate.
     */
// Not really needed.  Multiple same name/types would probably not be given to
// the OS.  All but one would probably use NvspDriver.noList.
//    i = strlen(mPart);
//    if (i)
//        mPart[i - 1] = 0; /* remove CR */
//    sprintf(mPart, "%s.%d", mPart, handle);
    /* Name of this sensor.
     * All sensors of the same "type" must have a different "name".
     */
    mSensorList->name = mPart;
    if (mVs)
        sprintf(path, "%s/vendor", mNvsDev->devSysfsPath());
    else
        sprintf(path, "%s/%s_vendor", mNvsDev->devSysfsPath(), mName);
    ret = sysfsStrRead(path, mVendor);
    if (ret > 0)
        mSensorList->vendor = mVendor;
    if (mVs)
        sprintf(path, "%s/version", mNvsDev->devSysfsPath());
    else
        sprintf(path, "%s/%s_version", mNvsDev->devSysfsPath(), mName);
    sysfsIntRead(path, &mSensorList->version);
    mSensorList->type = mType;
    /* SENSORS_DEVICE_API_VERSION_1_3 */
    if (mVs)
        sprintf(path, "%s/string_type", mNvsDev->devSysfsPath());
    else
        sprintf(path, "%s/%s_string_type", mNvsDev->devSysfsPath(), mName);
    ret = sysfsStrRead(path, mStringType);
    if (ret > 0) {
        mSensorList->stringType = mStringType;
    } else {
        if (mType > 0 && mType < (int)ARRAY_SIZE(nvsStringType))
            mSensorList->stringType = nvsStringType[mType];
        else
            mSensorList->stringType = NULL;
    }
    mSensorList->requiredPermission = NULL;
    if (mVs)
        sprintf(path, "%s/flags", mNvsDev->devSysfsPath());
    else
        sprintf(path, "%s/%s_flags", mNvsDev->devSysfsPath(), mName);
    sysfsIntRead(path, (int *)&mSensorList->flags);
    setSensorList();
    return 1; /* return count */
}

struct sensor_t *Nvs::getSensorListPtr()
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    return mSensorList;
}

char *Nvs::getSysfsPath()
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    return mNvsDev->devSysfsPath();
}

int Nvs::metaFlush(sensors_event_t *data)
{
    mFlush = false;
    mMetaDataEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
    *data = mMetaDataEvent;
    ALOGI_IF(SensorBase::INPUT_DATA || SensorBase::EXTRA_VERBOSE,
             "%s %s sensors_meta_data_event_t version=%d sensor=%d type=%d\n"
             "timestamp=%lld meta_data.what=%d meta_data.sensor=%d\n",
             __func__, mName,
             data->version,
             data->sensor,
             data->type,
             data->timestamp,
             data->meta_data.what,
             data->meta_data.sensor);
    return 1;
}

int Nvs::readMatrix()
{
    FILE *fp;
    char str[NVS_SYSFS_PATH_SIZE_MAX];

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    if (mVs)
        sprintf(str, "%s/matrix", mNvsDev->devSysfsPath());
    else
        sprintf(str, "%s/%s_matrix", mNvsDev->devSysfsPath(), mName);
    fp = fopen(str, "r");
    if (fp != NULL) {
        fscanf(fp, "%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd",
               &mMatrix[0], &mMatrix[1], &mMatrix[2], &mMatrix[3], &mMatrix[4],
               &mMatrix[5], &mMatrix[6], &mMatrix[7], &mMatrix[8]);
        fclose(fp);
        ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                 "%s %s = %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd\n",
                 __func__, str, mMatrix[0], mMatrix[1], mMatrix[2], mMatrix[3],
                  mMatrix[4], mMatrix[5], mMatrix[6], mMatrix[7], mMatrix[8]);
        return 0;
    }

    return -EINVAL;
}

int Nvs::getMatrix(int32_t handle, signed char *matrix)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d matrix=%s\n", __func__, handle, matrix);
    int ret = readMatrix();

    if (!ret)
        memcpy(matrix, &mMatrix, sizeof(mMatrix));
    return ret;
}

int Nvs::setMatrix(int32_t handle, signed char *matrix)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d matrix=%s\n", __func__, handle, matrix);
    memcpy(&mMatrix, matrix, sizeof(mMatrix));
    matrixValidate();
    return 0;
}

void Nvs::matrixValidate()
{
    unsigned int i;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    for (i = 0; i < sizeof(mMatrix); i++) {
        if (mMatrix[i]) {
            mMatrixEn = true;
            ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                     "%s %s mMatrixEn = true\n", __func__, mName);
            return;
        }
    }
    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
             "%s %s mMatrixEn = false\n", __func__, mName);
    mMatrixEn = false;
}

float Nvs::matrix(float x, float y, float z, unsigned int axis)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s x=%f y=%f z=%f axis=%u\n",
              __func__, x, y, z, axis);
    return ((mMatrix[0 + axis] == 1 ? x : (mMatrix[0 + axis] == -1 ? -x : 0)) +
            (mMatrix[3 + axis] == 1 ? y : (mMatrix[3 + axis] == -1 ? -y : 0)) +
            (mMatrix[6 + axis] == 1 ? z : (mMatrix[6 + axis] == -1 ? -z : 0)));
}

void Nvs::eventDataMatrix(unsigned int index)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s index=%u\n", __func__, index);
    if (mMatrixEn) {
        /* swap axis data based on matrix */
        float fVal[3];
        unsigned int i;

        for (i = index; i < index + 3; i++)
            fVal[i] = mEvent.data[i];
        for (i = index; i < index + 3; i++)
            mEvent.data[i] = matrix(fVal[0], fVal[1], fVal[2], i);
    }
}

int Nvs::eventDataScaleOffset(struct devChannel *dCh, float *fVal)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s dCh=%p *fVal=%f\n",
             __func__, dCh, *fVal);
    if (mCalibrationMode) {
        float scale = 0;
        float offset = 0;

        /* read scale and offset each time in calibration mode */
        sysfsFloatRead(dCh->attrPath[ATTR_SCALE], &scale);
        sysfsFloatRead(dCh->attrPath[ATTR_OFFSET], &offset);
        if (scale) {
            ALOGI("calibration: data=%f * scale=%f + offset=%f == %f\n",
                  *fVal, scale, offset, (*fVal * scale) + offset);
            *fVal *= scale;
            *fVal += offset;
        } else {
            ALOGI("calibration: data=%f * NO SCALE + offset=%f == %f\n",
                  *fVal, offset, *fVal + offset);
            *fVal += offset;
        }
    } else {
        if (dCh->attrFVal[ATTR_SCALE])
            *fVal *= dCh->attrFVal[ATTR_SCALE];
        if (dCh->attrFVal[ATTR_OFFSET])
            *fVal += dCh->attrFVal[ATTR_OFFSET];
    }
    return 0;
}

int Nvs::eventData(uint8_t *buf, int32_t handle)
{
    uint64_t chData;
    int64_t s64Data;
    float fVal;
    bool dataChange = false;
    int n;
    int i;

    for (i = 0; i < mNvsChN; i++) {
        n = mNvsDev->devGetChanData(mNvsCh[i], buf,
                                    &chData, sizeof(chData));
        if (n < 1)
            continue;

        switch (mNvsCh[i]->dataType) {
        case NVS_EVENT_DATA_TYPE_VEC: {
            if (mNvsCh[i]->dataIndex < 3) {
                s64Data = (int64_t)chData;
                fVal = (float)s64Data;
                mEvent.acceleration.v[mNvsCh[i]->dataIndex] = fVal;
                if (handle > 0)
                    /* apply offset and scale if not driven by fusion */
                    eventDataScaleOffset(mNvsCh[i],
                                 &mEvent.acceleration.v[mNvsCh[i]->dataIndex]);
            } else {
                mEvent.acceleration.status = (int8_t)chData;
            }
            if (i == mNvsChN - 1) {
                eventDataMatrix(0);
                ALOGI_IF(SensorBase::INPUT_DATA,
                         "%s %s sensors_event_t version=%d sensor=%d type=%d\n"
                         "timestamp=%lld  x=%f y=%f z=%f  status=%hhd\n",
                         __func__, mName,
                         mEvent.version,
                         mEvent.sensor,
                         mEvent.type,
                         mEvent.timestamp,
                         mEvent.acceleration.x,
                         mEvent.acceleration.y,
                         mEvent.acceleration.z,
                         mEvent.acceleration.status);
            }
            break;
        }

        case NVS_EVENT_DATA_TYPE_UNCAL: {
            s64Data = (int64_t)chData;
            fVal = (float)s64Data;
            if (mNvsCh[i]->dataIndex < 3)
                mEvent.uncalibrated_gyro.uncalib[mNvsCh[i]->dataIndex] = fVal;
            else if (mNvsCh[i]->dataIndex < 6)
                mEvent.uncalibrated_gyro.bias[mNvsCh[i]->dataIndex] = fVal;
            if (i == mNvsChN - 1) {
                eventDataMatrix(0);
                eventDataMatrix(3);
                ALOGI_IF(SensorBase::INPUT_DATA,
                         "%s %s sensors_event_t version=%d sensor=%d type=%d\n"
                         "timestamp=%lld  "
                         "uncalib: x=%f y=%f z=%f  bias: x=%f y=%f z=%f\n",
                         __func__, mName,
                         mEvent.version,
                         mEvent.sensor,
                         mEvent.type,
                         mEvent.timestamp,
                         mEvent.uncalibrated_gyro.x_uncalib,
                         mEvent.uncalibrated_gyro.y_uncalib,
                         mEvent.uncalibrated_gyro.z_uncalib,
                         mEvent.uncalibrated_gyro.x_bias,
                         mEvent.uncalibrated_gyro.y_bias,
                         mEvent.uncalibrated_gyro.z_bias);
            }
            break;
        }

        case NVS_EVENT_DATA_TYPE_HEART: {
            s64Data = (int64_t)chData;
            if (mNvsCh[i]->dataIndex) {
                mEvent.heart_rate.status = (int8_t)s64Data;
            } else {
                fVal = (float)s64Data;
                mEvent.heart_rate.bpm = fVal;
            }
            if (i == mNvsChN - 1)
                ALOGI_IF(SensorBase::INPUT_DATA,
                         "%s %s sensors_event_t version=%d sensor=%d type=%d\n"
                         "timestamp=%lld  bpm=%f  status=%hhd\n",
                         __func__, mName,
                         mEvent.version,
                         mEvent.sensor,
                         mEvent.type,
                         mEvent.timestamp,
                         mEvent.heart_rate.bpm,
                         mEvent.heart_rate.status);
            break;
        }

        default:
            if (n == (int)(sizeof(uint64_t))) {
                mEvent.data[mNvsCh[i]->dataIndex] = (uint64_t)chData;
                if (i == mNvsChN - 1)
                    ALOGI_IF(SensorBase::INPUT_DATA,
                             "%s %s sensors_event_t version=%d sensor=%d "
                             "type=%d\ntimestamp=%lld  data[0]=%llu "
                             "data[1]=%llu data[2]=%llu data[3]=%llu\n",
                             __func__, mName,
                             mEvent.version,
                             mEvent.sensor,
                             mEvent.type,
                             mEvent.timestamp,
                             mEvent.data[0],
                             mEvent.data[1],
                             mEvent.data[2],
                             mEvent.data[3]);
                break;
            } else {
                s64Data = (int64_t)chData;
                fVal = (float)s64Data;
                if (handle > 0)
                    /* apply offset and scale if not driven by fusion */
                    eventDataScaleOffset(mNvsCh[i], &fVal);
                if (mEvent.data[mNvsCh[i]->dataIndex] != fVal)
                    dataChange = true;
                mEvent.data[mNvsCh[i]->dataIndex] = fVal;
                if (i == mNvsChN - 1) {
                    /* drop data if same data for on-change sensor
                     * (minDelay == 0 || SENSOR_FLAG_ON_CHANGE_MODE)
                     */
// FIXME: remove backward compatible test for (mSensorList->minDelay == 0)
                    if ((mSensorList->minDelay == 0) || (mSensorList->flags &
                                                 SENSOR_FLAG_ON_CHANGE_MODE)) {
                        if (!dataChange && !mFirstEvent && !mCalibrationMode) {
                            /* data hasn't changed for on-change sensor
                             * and first already sent
                             * and not in calibration mode
                             */
                            ALOGI_IF(SensorBase::INPUT_DATA,
                                     "%s %s SAME DATA DROPPED\n"
                                     "sensors_event_t version=%d sensor=%d "
                                     "type=%d\ntimestamp=%lld  data[0]=%f "
                                     "data[1]=%f data[2]=%f data[3]=%f\n",
                                     __func__, mName,
                                     mEvent.version,
                                     mEvent.sensor,
                                     mEvent.type,
                                     mEvent.timestamp,
                                     mEvent.data[0],
                                     mEvent.data[1],
                                     mEvent.data[2],
                                     mEvent.data[3]);
                            return 0;
                        }
                    }
                    ALOGI_IF(SensorBase::INPUT_DATA,
                             "%s %s sensors_event_t version=%d sensor=%d "
                             "type=%d\ntimestamp=%lld  "
                             "data[0]=%f data[1]=%f data[2]=%f data[3]=%f\n",
                             __func__, mName,
                             mEvent.version,
                             mEvent.sensor,
                             mEvent.type,
                             mEvent.timestamp,
                             mEvent.data[0],
                             mEvent.data[1],
                             mEvent.data[2],
                             mEvent.data[3]);
                }
            }
        }
    }

    return 1;
}

int Nvs::processEvent(sensors_event_t *data, uint8_t *buf, int32_t handle)
{
    int nEvents = 0;
    int ret;

    if (!mNvsChN)
        return 0;

    if (mFlush && !mSensorList->fifoMaxEventCount)
        /* if batch not supported but still need to send meta packet */
        return metaFlush(data);

    ret = mNvsDev->devGetTimestamp(mNvsCh[0], buf, &mEvent.timestamp);
    if (ret <= 0)
        /* NVS requires a timestamp, but if there isn't one, then batch/flush
         * and the ability to detect invalid data is disabled.
         */
        mEvent.timestamp = getTimestamp();
    if (!mEvent.timestamp) {
        /* The absence of a timestamp is because either a flush was done or the
         * sensor data is currently invalid.
         * If we've sent down a flush command (mFlush), then we'll send up a
         * meta data event.  Otherwise, the data stream stops here as data is
         * just dropped while there is no timestamp.
         */
        if (mFlush)
            return metaFlush(data);

        ALOGI_IF(SensorBase::INPUT_DATA || SensorBase::ENG_VERBOSE,
                 "%s %s data dropped - no timestamp\n",
                 __func__, mName);
        return 0;
    }

    if (mEnable) {
        nEvents = eventData(buf, handle);
        if (nEvents > 0) {
            *data = mEvent;
            mFirstEvent = false;
            if ((mSensorList->minDelay < 0) || (mSensorList->flags &
                                                SENSOR_FLAG_ONE_SHOT_MODE))
                /* this is a one-shot and is disabled after one event */
                mEnable = 0;
        }
    }
    return nEvents;
}

int Nvs::readEvents(sensors_event_t *data, int count, int32_t handle)
{
    return mNvsDev->devGetEventData(data, count, handle);
}

int Nvs::readRaw(int32_t handle, int *data, int channel)
{
    int ret;

    if (channel >= mNvsChN)
        return -EINVAL;

    if (channel < 0)
        channel = 0;

    ret = sysfsIntRead(mNvsCh[channel]->attrPath[ATTR_RAW], data);
    ALOGI_IF(SensorBase::INPUT_DATA,
             "%s handle=%d channel=%d data=%d err=%d\n",
             __func__, handle, channel, *data, ret);
    return ret;
}

