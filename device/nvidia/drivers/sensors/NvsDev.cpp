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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include "NvsDev.h"


NvsDev::NvsDev(Nvs *link)
    : mLinkRoot(link),
      mLink(NULL),
      mSysfsPath(NULL),
      mDevCh(NULL),
      mDevChN(0),
      mFd(-1)
{
}

NvsDev::~NvsDev()
{
}

int NvsDev::devChanAble(int32_t handle, int channel, int enable)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d channel=%d enable=%d\n",
             __func__, handle, channel, enable);
    return 0;
}

int NvsDev::devGetEventData(sensors_event_t *data, int count, int32_t handle)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d data=%p count=%d\n", __func__, handle, data, count);
    return 0;
}

int NvsDev::devGetChanData(struct devChannel *devCh, uint8_t *buf,
                           void *data, unsigned int dataSize)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s devCh=%p buf=%p data=%p dataSize=%u\n",
             __func__, devCh, buf, data, dataSize);
    return 0;
}

int NvsDev::devGetTimestamp(struct devChannel *devCh,
                            uint8_t *buf, int64_t *ts)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s devCh=%p buf=%p ts=%p\n",
             __func__, devCh, buf, ts);
    return -EINVAL;
}

void NvsDev::devSetLinkRoot(Nvs *link)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s link=%p\n", __func__, link);
    mLinkRoot = link;
}

char *NvsDev::devSysfsPath()
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s\n", __func__);
    return mSysfsPath;
}

int NvsDev::devFdInit(struct pollfd *pollFd, int32_t handle)
{
    int flags;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s pollFd=%p handle=%d\n", __func__, pollFd, handle);
    if (mFd >= 0) {
        pollFd->fd = mFd;
        flags = fcntl(pollFd->fd, F_GETFL, 0);
        flags |= O_NONBLOCK;
        fcntl(pollFd->fd, F_SETFL, flags);
        pollFd->events = POLLIN;
        pollFd->revents = 0;
    }
    ALOGI_IF(SensorBase::ENG_VERBOSE,
             "%s handle=%d pollFd=%p mFd=%d\n", __func__, handle, pollFd, mFd);
    return mFd;
}

int NvsDev::devFd(int32_t handle)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s handle=%d\n", __func__, handle);
    return mFd;
}

int NvsDev::devGetChanN(const char *devName)
{
    int n;
    int i;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s devName=%s\n", __func__, devName);
    if (mDevChN <= 0) {
        ALOGE("%s ERR: no device channels\n", __func__);
        return -EINVAL;
    }

    if (devName == NULL)
        return mDevChN;

    n = 0;
    for (i = 0; i < mDevChN; i++) {
        if (!(strcmp(devName, mDevCh[i].devName)))
            n++;
    }
    return n;
}

int NvsDev::devGetChanInfo(const char *devName,
                           struct devChannel **devCh)
{
    int n;
    int i;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s devName=%s devCh=%p\n", __func__, devName, devCh);
    if (!mDevChN) {
        ALOGE("%s ERR: no device channels\n", __func__);
        return -EINVAL;
    }

    if (devName == NULL) {
        for (i = 0; i < mDevChN; i++)
            devCh[i] = &mDevCh[i];
        return mDevChN;
    }

    n = 0;
    for (i = 0; i < mDevChN; i++) {
        if (!(strcmp(devName, mDevCh[i].devName))) {
            devCh[n] = &mDevCh[i];
            n++;
        }
    }
    return n;
}


int sysfsStrRead(char *path, char *str)
{
    char buf[64] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -ENOENT;

    ret = read(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        strcpy(str, buf);
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=%s  ret=%d\n",
                 __func__, path, str, ret);
    } else {
        ALOGE_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=ERR: %d  ret=%d\n",
                 __func__, path, -errno, ret);
        ret = -EIO;
    }
    return ret;
}

int sysfsFloatRead(char *path, float *fVal)
{
    char buf[32] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -ENOENT;

    ret = read(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        *fVal = atof(buf);
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=%f  ret=%d\n",
                 __func__, path, *fVal, ret);
        ret = 0;
    } else {
        ALOGE_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=ERR: %d  ret=%d\n",
                 __func__, path, -errno, ret);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

int sysfsFloatWrite(char *path, float fVal)
{
    char buf[32] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return -ENOENT;

    sprintf(buf, "%f", fVal);
    ret = write(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %f -> %s ret=%d\n",
                 __func__, fVal, path, ret);
        ret = 0;
    } else {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %f -> %s ret=%d ERR: %d\n",
                 __func__, fVal, path, ret, -errno);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

int sysfsIntRead(char *path, int *iVal)
{
    char buf[16] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -ENOENT;

    ret = read(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        *iVal = atoi(buf);
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=%d  ret=%d\n",
                 __func__, path, *iVal, ret);
        ret = 0;
    } else {
        ALOGE_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=ERR: %d  ret=%d\n",
                 __func__, path, -errno, ret);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

int sysfsIntWrite(char *path, int iVal)
{
    char buf[16] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return -ENOENT;

    sprintf(buf, "%d", iVal);
    ret = write(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %d -> %s ret=%d\n",
                 __func__, iVal, path, ret);
        ret = 0;
    } else {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %d -> %s ret=%d ERR: %d\n",
                 __func__, iVal, path, ret, -errno);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

