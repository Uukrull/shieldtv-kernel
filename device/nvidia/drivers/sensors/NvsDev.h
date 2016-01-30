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


#ifndef NVSDEV_H
#define NVSDEV_H

#include "Nvs.h"

class Nvs;

class NvsDev {

    void NvsbRemove();

protected:
    Nvs *mLinkRoot;
    Nvs *mLink;
    char *mSysfsPath;
    struct devChannel *mDevCh;
    int mDevChN;
    int mFd;

public:
    NvsDev(Nvs *link);
    virtual ~NvsDev();

    void devSetLinkRoot(Nvs *link);
    char *devSysfsPath();
    int devFd(int32_t handle);
    virtual int devFdInit(struct pollfd *pollFd, int32_t handle);
    virtual int devGetChanN(const char *devName);
    virtual int devGetChanInfo(const char *devName,
                               struct devChannel **devChInf);
    virtual int devChanAble(int32_t handle, int channel, int enable);
    virtual int devGetEventData(sensors_event_t *data, int count,
                                int32_t handle);
    virtual int devGetChanData(struct devChannel *devCh, uint8_t *buf,
                               void *data, unsigned int dataSize);
    virtual int devGetTimestamp(struct devChannel *devCh,
                                uint8_t *buf, int64_t *ts);
};

int sysfsStrRead(char *path, char *str);
int sysfsFloatRead(char *path, float *fVal);
int sysfsFloatWrite(char *path, float fVal);
int sysfsIntRead(char *path, int *iVal);
int sysfsIntWrite(char *path, int iVal);

#endif  // NVSDEV_H

