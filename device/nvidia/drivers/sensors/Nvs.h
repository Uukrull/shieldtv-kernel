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


#ifndef NVS_H
#define NVS_H

#include <errno.h>
#include <linux/input.h>
#include <hardware/sensors.h>
#include <utils/Vector.h>
#include "SensorBase.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif
#define NVS_SYSFS_PATH_SIZE_MAX         (256)
#define NVS_FLUSH_CMD_FLUSH             (1.0f)

enum NVS_DRIVER_TYPE {
    NVS_DRIVER_TYPE_IIO = 0,
    NVS_DRIVER_TYPE_INPUT,
    NVS_DRIVER_TYPE_RELAY,
    NVS_DRIVER_TYPE_N
};

enum NVS_EVENT_DATA_TYPE {
    NVS_EVENT_DATA_TYPE_UNKNOWN = 0,
    NVS_EVENT_DATA_TYPE_FLOAT,
    NVS_EVENT_DATA_TYPE_U64,
    NVS_EVENT_DATA_TYPE_VEC,
    NVS_EVENT_DATA_TYPE_UNCAL,
    NVS_EVENT_DATA_TYPE_BIAS,
    NVS_EVENT_DATA_TYPE_HEART,
    NVS_EVENT_DATA_TYPE_N
};

enum NVSB_ATTR {
    ATTR_BATCH_FLAGS = 0,
    ATTR_BATCH_PERIOD,
    ATTR_BATCH_TIMEOUT,
    ATTR_DELAY,
    ATTR_FLUSH,
    ATTR_MAX_RANGE,
    ATTR_OFFSET,
    ATTR_RAW,
    ATTR_RESOLUTION,
    ATTR_SCALE,
    ATTR_N
};

struct attrPath {
    enum NVSB_ATTR attr;
    const char *attrName;
};

struct devChannel {
    int handle;                         /* sensor handle */
    char *devName;                      /* device name */
    void *drvCh;                        /* driver channel info */
    enum NVS_EVENT_DATA_TYPE dataType;  /* event data type */
    unsigned int dataIndex;             /* event data type index */
    unsigned int enabled;               /* channel enable status */
    char *attrPath[ATTR_N];             /* sysfs attribute paths */
    bool attrShared[ATTR_N];            /* if sysfs attribute is chan shared */
    bool attrCached[ATTR_N];            /* attribute is cached */
    float attrFVal[ATTR_N];             /* float read from sysfs attribute */
};

struct NvspDriver {
    /* Setting the kernel device name enables the autodetection of a kernel
     * device and will provide the device number for the driver.
     * This can be set to NULL if the driver has its own device detection or
     * driver is not associated with a physical device.
     */
    const char *devName;                /* name of kernel device */
    /* If this is not a single device then this must be 0 to turn off multiple
     * device detection.  Multiple device detection allows multiple single
     * devices of the same type and is part of the driver auto detection.
     */
    int devType;                        /* OS sensor type (for auto detect) */
    enum NVS_DRIVER_TYPE driverType;    /* driver type (see NVS_DRIVER_TYPE) */
    /* There may be a need for sensor data but do not want the sensor to be
     * populated in the struct sensor_t sensor list.  An example of this is
     * where the fusion driver needs the gyro device temperature for gyro
     * calculations.
     * Since the sensor will no longer be controlled by the OS, it's assumed
     * another entity will have control (i.g. fusion).  NVSP will still assign
     * it a dynamic handler and be aware of it on the low end of the SW stack
     * to pass its data to the driver.
     * Note: this feature is not compatible with static entries.
     */
    bool noList;                        /* set if to omit from sensor list */
    /* When fusionClient is set, all OS calls go through the fusion driver */
    bool fusionClient;                  /* set if driver uses fusion */
    bool fusionDriver;                  /* set if this is the fusion driver */
    /* function to install driver */
    SensorBase *(*newDriver)(int devNum, enum NVS_DRIVER_TYPE driverType);
    /* The purpose of the staticHandle and numStaticHandle is to load drivers
     * for static entries in the nvspSensorList.
     * staticHandle = the static entry of nvspSensorList[]->handle
     *                (nvspSensorList[index] => index + 1)
     * numStaticHandle = the number of sequential static entries to replace
     *                   (will be the limit variable in get_sensor_list)
     * Example:
     * struct NvspDriver nvspDriverList[] = {
     * {
     *  .devName                = NULL,
     *  .newDriver              = function to load driver without device,
     *  .staticHandle           = index to static entry in nvspSensorList + 1,
     *  .numStaticHandle        = number of static entries this driver is for,
     * },
     * Note that this mechanism also allows static entries in nvspSensorList to
     * be replaced by a device/driver using the device autodetect mechanism.
     * Example:
     * static struct sensor_t nvspSensorList[SENSOR_COUNT_MAX] = {
     *  {
     *   .name                   = "NAME",
     *   .vendor                 = "VENDOR",
     *   .version                = 1,
     *   .handle                 = 1,
     *   .type                   = SENSOR_TYPE_,
     *   .maxRange               = 1.0f,
     *   .resolution             = 1.0f,
     *   .power                  = 1.0f,
     *   .minDelay               = 10000,
     *  },
     * };
     * struct NvspDriver nvspDriverList[] = {
     * {
     *  .devName                = device name,
     *  .newDriver              = driver load function with associated device,
     *  .staticHandle           = 1,
     *  .numStaticHandle        = 1,
     * },
     * If devName is found, the driver will be loaded for the static entry
     * index 0 (handle 1).
     * In either case, the get_sensor_list will still be called where
     * index = staticHandle - 1
     * limit = numStaticHandle
     * allowing the driver to either replace the entry or pick up the handle.
     */
    int staticHandle;                   /* if dev found replace static list */
    int numStaticHandle;              /* number of static entries to replace */
};


class NvsDev;

class Nvs: public SensorBase {

    void NvsRemove();

protected:
    const char *mName;
    const char **mChanNames;
    int mType;
    Nvs *mLink;
    NvsDev *mNvsDev;
    struct devChannel **mNvsCh;
    struct sensor_t *mSensorList;
    int mEnable;
    int mNvsChN;
    bool mVs;
    bool mFlush;
    bool mHasPendingEvent;
    bool mFirstEvent;
    bool mCalibrationMode;
    bool mMatrixEn;
    signed char mMatrix[9];
    char mPart[64];
    char mVendor[128];
    char mStringType[128];
    sensors_event_t mEvent;
    sensors_meta_data_event_t mMetaDataEvent;

    virtual int setAttr(int32_t handle, int channel,
                        enum NVSB_ATTR attr, float fVal);
    virtual int getAttr(int32_t handle, int channel,
                        enum NVSB_ATTR attr, float *fVal);
    virtual void setSensorList();
    virtual int metaFlush(sensors_event_t *data);
    virtual float matrix(float x, float y, float z, unsigned int axis);
    virtual void matrixValidate();
    virtual int readMatrix();
    virtual void eventDataMatrix(unsigned int index);
    virtual int eventDataScaleOffset(struct devChannel *dCh, float *fVal);
    virtual int eventData(uint8_t *buf, int32_t handle);

public:
    Nvs(int devNum,
        const char *name,
        int type,
        enum NVS_DRIVER_TYPE driverType,
        Nvs *link = NULL);
    virtual ~Nvs();

    virtual int getFd(int32_t handle = -1);
    virtual int initFd(struct pollfd *pollFd, int32_t handle = -1);
    virtual int enable(int32_t handle, int enabled, int channel = -1);
    virtual int getEnable(int32_t handle, int channel = -1);
    virtual int setDelay(int32_t handle, int64_t ns, int channel = -1);
    virtual int64_t getDelay(int32_t handle, int channel = -1);
    virtual int setMaxRange(int32_t handle, float maxRange, int channel = -1);
    virtual int getMaxRange(int32_t handle, float *maxRange, int channel = -1);
    virtual int setOffset(int32_t handle, float offset, int channel = -1);
    virtual int getOffset(int32_t handle, float *offset, int channel = -1);
    virtual int setResolution(int32_t handle, float resolution,
                              int channel = -1);
    virtual int getResolution(int32_t handle, float *resolution,
                              int channel = -1);
    virtual int setScale(int32_t handle, float scale, int channel = -1);
    virtual int getScale(int32_t handle, float *scale, int channel = -1);
    virtual int getSensorList(struct sensor_t *list, int index, int limit);
    virtual struct sensor_t *getSensorListPtr();
    virtual char *getSysfsPath();
    virtual bool hasPendingEvents() { return false; };
    virtual int batch(int handle, int flags,
                      int64_t sampling_period_ns,
                      int64_t max_report_latency_ns);
    virtual int flush(int32_t handle);
    virtual int getMatrix(int32_t handle, signed char *matrix);
    virtual int setMatrix(int32_t handle, signed char *matrix);
    virtual int readRaw(int32_t handle, int *data, int channel = -1);
    virtual int readEvents(sensors_event_t *data, int count,
                           int32_t handle = -1);
    virtual int processEvent(sensors_event_t *data, uint8_t *buf,
                             int32_t handle);
    Nvs *(getLink)(void);
};

#endif  // NVS_H

