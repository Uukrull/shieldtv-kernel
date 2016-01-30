/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (c) 2008 Jonathan Cameron
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


#ifndef NVSIIO_H
#define NVSIIO_H

#include "NvsDev.h"
#include "Nvs.h"


struct iioChannel {
    char *preName;                      /* PRE_name */
    char *postName;                     /* name_POST */
    char *fullName;                     /* (PRE_)name(_POST) */
    char *pathEn;                       /* channel enable sysfs path */
    unsigned int index;                 /* channel index */
    unsigned int bigendian;             /* scan_elements/<full_name>_type */
    unsigned int sign;                  /* scan_elements/<full_name>_type */
    unsigned int realbits;              /* scan_elements/<full_name>_type */
    unsigned int shift;                 /* scan_elements/<full_name>_type */
    unsigned int bytes;                 /* number of channel data bytes */
    uint64_t mask;                      /* mask for valid data */
    unsigned int location;              /* data alignment in buffer */
};

static struct attrPath attrPathTbl[] = {
    { ATTR_BATCH_FLAGS, "batch_flags", },
    { ATTR_BATCH_PERIOD, "batch_period", },
    { ATTR_BATCH_TIMEOUT, "batch_timeout", },
    { ATTR_DELAY, "sampling_frequency", },
    { ATTR_FLUSH, "flush", },
    { ATTR_MAX_RANGE, "peak_raw", },
    { ATTR_OFFSET, "offset", },
    { ATTR_RAW, "raw", },
    { ATTR_RESOLUTION, "peak_scale", },
    { ATTR_SCALE, "scale", }
};

struct dataType {
    enum NVS_EVENT_DATA_TYPE dataType;
    unsigned int dataIndex;
    const char *attrName;
};

static struct dataType dataTypeTbl[] = {
    { NVS_EVENT_DATA_TYPE_VEC, 0, "x", },
    { NVS_EVENT_DATA_TYPE_VEC, 1, "y", },
    { NVS_EVENT_DATA_TYPE_VEC, 2, "z", },
    { NVS_EVENT_DATA_TYPE_UNCAL, 0, "x_uncalib", },
    { NVS_EVENT_DATA_TYPE_UNCAL, 1, "y_uncalib", },
    { NVS_EVENT_DATA_TYPE_UNCAL, 2, "z_uncalib", },
    { NVS_EVENT_DATA_TYPE_BIAS, 3, "x_bias", },
    { NVS_EVENT_DATA_TYPE_BIAS, 4, "y_bias", },
    { NVS_EVENT_DATA_TYPE_BIAS, 5, "z_bias", },
    { NVS_EVENT_DATA_TYPE_HEART, 0, "bpm", }
};

static const char *strTimestamp = "in_timestamp";


class NvsIio: public NvsDev {

    struct devChannel *mDevChTs;
    int mBufSize;
    uint8_t *mBuf;
    char *mPathBufLength;
    char *mPathBufEnable;
    char *mPathEnable;
    int mDbgFdN;
    int mDbgBufSize;

    void NvsIioRemove();
    int bufDisable();
    int bufEnable(bool force);
    void dbgChan();

public:
    NvsIio(Nvs *link, int devNum);
    virtual ~NvsIio();

    virtual int devChanAble(int32_t handle, int channel, int enable);
    virtual int devGetChanN(const char *devName);
    virtual int devGetChanInfo(const char *devName,
                               struct devChannel **devChInf);
    virtual int devGetEventData(sensors_event_t *data, int count,
                                int32_t handle);
    virtual int devGetChanData(struct devChannel *dCh, uint8_t *buf,
                               void *data, unsigned int dataSize);
    virtual int devGetTimestamp(struct devChannel *dCh,
                                uint8_t *buf, int64_t *ts);
};

#endif  // NVSIIO_H

