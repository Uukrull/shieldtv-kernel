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

/* NvsIio is the interface between NVS and the IIO kernel. */
/* The NVS (NVidia Sensor) implementation of scan_elements enable/disable works
 * using the IIO mechanisms with one additional NVS enhancement for disable:
 * To enable, the NVS HAL will:
 * 1. Disable buffer
 * 2. Enable channels
 * 3. Calculate buffer alignments based on enabled channels
 * 4. Enable buffer
 * It is expected that the NVS kernel driver will detect the channels enabled
 * and enable the device using the IIO iio_buffer_setup_ops.
 * To disable, the NVS HAL will:
 * 1. Disable buffer
 * 2. Disable channels
 * 3. Calculate buffer alignments based on remaining enabled channels, if any
 * 4. If (one or more channels are enabled)
 *        4a. Enable buffer
 *    else
 *        4b. Disable master enable
 * It is expected that the master enable will be enabled as part of the
 * iio_buffer_setup_ops.
 * The NVS sysfs attribute for the master enable is "enable".
 * The master enable is an enhancement of the standard IIO enable/disable
 * procedure.  Consider a device that contains multiple sensors.  To enable all
 * the sensors with the standard IIO enable mechanism, the device would be
 * powered off and on for each sensor that was enabled.  By having a master
 * enable, the device does not have to be powered down for each buffer
 * disable but only when the master enable is disabled, thereby improving
 * efficiency.
 */


#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include "NvsIio.h"


NvsIio::NvsIio(Nvs *link, int devNum)
    : NvsDev(link),
      mBufSize(0),
      mBuf(NULL),
      mPathBufLength(NULL),
      mPathBufEnable(NULL),
      mPathEnable(NULL)
{
    DIR *dp = NULL;
    FILE *fp;
    const struct dirent *d;
    struct devChannel devCh;
    struct devChannel *dCh;
    struct iioChannel *iCh;
    struct iioChannel *iCh2;
    char pathScan[NVS_SYSFS_PATH_SIZE_MAX];
    char path[NVS_SYSFS_PATH_SIZE_MAX];
    int i;
    int j;
    int ret;

    /* init sysfs root path */
    ret = asprintf(&mSysfsPath, "/sys/bus/iio/devices/iio:device%d", devNum);
    if (ret < 0) {
        ALOGE("%s ERR: asprintf mSysfsPath\n", __func__);
        goto errExit;
    }

    sprintf(path, "/dev/iio:device%d", devNum);
    mFd = open(path, O_RDONLY);
    if (mFd < 0) {
        ALOGE("%s open %s ERR: %d\n", __func__, path, -errno);
        goto errExit;
    }

    /* open scan_elements */
    sprintf(pathScan, "%s/scan_elements", mSysfsPath);
    dp = opendir(pathScan);
    if (dp == NULL)
        goto errExit;

    /* count number of channels */
    while (d = readdir(dp), d != NULL) {
        if (!(strcmp(d->d_name + strlen(d->d_name) - strlen("_en"), "_en")))
            mDevChN++;
    }
    if (!mDevChN)
        goto errExit;

    /* allocate channel info memory */
    mDevCh = new devChannel[mDevChN];
    memset(mDevCh, 0, sizeof(struct devChannel) * mDevChN);
    for (i = 0; i < mDevChN; i++) {
        mDevCh[i].drvCh = new iioChannel;
        memset(mDevCh[i].drvCh, 0, sizeof(struct iioChannel));
    }
    /* create master enable path */
    ret = asprintf(&mPathEnable, "%s/enable", mSysfsPath);
    if (ret < 0)
        goto errExit;

    /* create buffer enable path */
    ret = asprintf(&mPathBufEnable, "%s/buffer/enable", mSysfsPath);
    if (ret < 0)
        goto errExit;

    /* create buffer length path */
    ret = asprintf(&mPathBufLength, "%s/buffer/length", mSysfsPath);
    if (ret < 0)
        goto errExit;

    /* build channel info */
    rewinddir(dp);
    i = 0;
    while (d = readdir(dp), d != NULL) {
        if (!(strcmp(d->d_name + strlen(d->d_name) - strlen("_en"), "_en"))) {
            dCh = &mDevCh[i++];
            iCh = (struct iioChannel *)dCh->drvCh;
            /* channel sysfs enable path */
            ret = asprintf(&iCh->pathEn,
                           "%s/%s", pathScan, d->d_name);
            if (ret < 0)
                goto errExit;

            /* init channel enable status */
            fp = fopen(iCh->pathEn, "r");
            if (fp == NULL)
                goto errExit;

            fscanf(fp, "%u", &dCh->enabled);
            fclose(fp);
            /* channel full name */
            iCh->fullName = strndup(d->d_name,
                                    strlen(d->d_name) - strlen("_en"));
            if (iCh->fullName != NULL) {
                char *str;
                char *pre;
                char *name;
                char *post;

                /* channel pre name */
                str = strdup(iCh->fullName);
                if (str == NULL)
                    goto errExit;

                pre = strtok(str, "_\0");
                /* channel name */
                name = strtok(NULL, "_\0");
                if (name == NULL) {
                    free(str);
                    goto errExit;
                }

                dCh->devName = strdup(name);
                ret = asprintf(&iCh->preName, "%s_%s", pre, name);
                if (ret < 0) {
                    free(str);
                    goto errExit;
                }

                post = strtok(NULL, "\0");
                if (post != NULL)
                    iCh->postName = strdup(post);
                free(str);
                if (ret < 0)
                    goto errExit;

            } else {
                goto errExit;
            }

            /* channel index */
            sprintf(path, "%s/%s_index", pathScan, iCh->fullName);
            fp = fopen(path, "r");
            if (fp == NULL)
                goto errExit;

            fscanf(fp, "%u", &iCh->index);
            fclose(fp);
            /* channel type info */
            sprintf(path, "%s/%s_type", pathScan, iCh->fullName);
            fp = fopen(path, "r");
            if (fp != NULL) {
                char endian;
                char sign;

                fscanf(fp, "%ce:%c%u/%u>>%u",
                       &endian,
                       &sign,
                       &iCh->realbits,
                       &iCh->bytes,
                       &iCh->shift);
                fclose(fp);
                iCh->bytes /= 8;
                if (endian == 'b')
                    iCh->bigendian = 1;
                else
                    iCh->bigendian = 0;
                if (sign == 's')
                    iCh->sign = 1;
                else
                    iCh->sign = 0;
                if (iCh->realbits == 64)
                    iCh->mask = ~0;
                else
                    iCh->mask = (1LL << iCh->realbits) - 1;
            } else {
                goto errExit;
            }

            if (strcmp(iCh->fullName, strTimestamp)) {
                /* set dataType and dataIndex */
                if (iCh->postName) {
                    for (j = 0; j < (int)ARRAY_SIZE(dataTypeTbl); j++) {
                        if (strlen(iCh->postName) == strlen(dataTypeTbl[j].
                                                            attrName)) {
                            if (!strcmp(iCh->postName,
                                        dataTypeTbl[j].attrName)) {
                                dCh->dataType = dataTypeTbl[j].dataType;
                                dCh->dataIndex = dataTypeTbl[j].dataIndex;
                                break;
                            }
                        }
                    }
                } else {
                    j = (int)(ARRAY_SIZE(dataTypeTbl));
                }
                if (j >= (int)(ARRAY_SIZE(dataTypeTbl))) {
                    /* figure out the data type based on data size */
                    if (iCh->bytes <= sizeof(uint32_t))
                        dCh->dataType = NVS_EVENT_DATA_TYPE_FLOAT;
                    else
                        dCh->dataType = NVS_EVENT_DATA_TYPE_U64;
                }
                /* IIO sysfs attribute paths */
                for (j = 0; j < ATTR_N; j++) {
                    dCh->attrFVal[j] = 0;
                    sprintf(path, "%s/%s_%s", mSysfsPath,
                            iCh->preName, attrPathTbl[j].attrName);
                    ret = access(path, F_OK);
                    if (!ret) {
                        dCh->attrShared[j] = true;
                        dCh->attrPath[j] = strdup(path);
                        continue;
                    }

                    dCh->attrShared[j] = false;
                    sprintf(path, "%s/%s_%s", mSysfsPath,
                            iCh->fullName, attrPathTbl[j].attrName);
                    ret = access(path, F_OK);
                    if (!ret) {
                        dCh->attrPath[j] = strdup(path);
                        continue;
                    }

                    dCh->attrPath[j] = NULL;
                }

                /* read offset value */
                ret = sysfsFloatRead(dCh->attrPath[ATTR_OFFSET],
                                     &dCh->attrFVal[ATTR_OFFSET]);
                if (ret)
                    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                             "%s: %s=read ERR: %d\n",
                             __func__, dCh->attrPath[ATTR_OFFSET], ret);
                else
                    dCh->attrCached[ATTR_OFFSET] = true;
                /* read scale value */
                ret = sysfsFloatRead(dCh->attrPath[ATTR_SCALE],
                                     &dCh->attrFVal[ATTR_SCALE]);
                if (ret)
                    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                             "%s: %s=read ERR: %d\n",
                             __func__, dCh->attrPath[ATTR_SCALE], ret);
                else
                    dCh->attrCached[ATTR_SCALE] = true;
            } else {
                mDevChTs = dCh;
                /* make sure timestamp is enabled */
                sysfsIntWrite(iCh->pathEn, 1);
            }
        }
    }

    closedir(dp);
    if (mDevChN > 1) {
        /* reorder so that the array is in index order */
        for (i = 0; i < mDevChN; i++) {
            for (j = 0; j < (mDevChN - 1); j++) {
                iCh = (struct iioChannel *)mDevCh[j].drvCh;
                iCh2 = (struct iioChannel *)mDevCh[j + 1].drvCh;
                if (iCh->index > iCh2->index) {
                    devCh = mDevCh[j + 1];
                    mDevCh[j + 1] = mDevCh[j];
                    mDevCh[j] = devCh;
                }
            }
        }
    }
    /* scan alignments and buffer size */
    j = 0;
    for (i = 0; i < mDevChN; i++) {
        iCh = (struct iioChannel *)mDevCh[i].drvCh;
        if (!(j % iCh->bytes))
            iCh->location = j;
        else
            iCh->location = j - j % iCh->bytes + iCh->bytes;
        j = iCh->location + iCh->bytes;
    }
    mBufSize = j;
    mBuf = new uint8_t[mBufSize];
    if (SensorBase::EXTRA_VERBOSE)
        dbgChan();
    return;

errExit:
    NvsIioRemove();
    if (dp != NULL)
        closedir(dp);
}

NvsIio::~NvsIio()
{
    NvsIioRemove();
}

void NvsIio::NvsIioRemove()
{
    struct iioChannel *iCh;
    int i;
    int j;

    if (mSysfsPath != NULL)
        free(mSysfsPath);
    if (mDevCh) {
        for (i = 0; i < mDevChN; i++) {
            if (mDevCh[i].devName != NULL)
                free(mDevCh[i].devName);
            iCh = (struct iioChannel *)mDevCh[i].drvCh;
            if (iCh->preName != NULL)
                free(iCh->preName);
            if (iCh->postName != NULL)
                free(iCh->postName);
            if (iCh->fullName != NULL)
                free(iCh->fullName);
            if (iCh->pathEn != NULL)
                free(iCh->pathEn);
            for (j = 0; j < ATTR_N; j++) {
                if (mDevCh[i].attrPath[j] != NULL)
                    free(mDevCh[i].attrPath[j]);
            }
        }
        delete mDevCh;
    }
    if (mFd >= 0)
        close(mFd);
    /* IIO class specific */
    if (mPathEnable)
        free(mPathEnable);
    if (mPathBufEnable)
        free(mPathBufEnable);
    if (mPathBufLength)
        free(mPathBufLength);
    delete mBuf;
}

int NvsIio::bufDisable()
{
    int ret;

    ret = sysfsIntWrite(mPathBufEnable, 0);
    if (ret)
        ALOGE("%s 0 -> %s ERR: %d\n", __func__, mPathBufEnable, ret);
    return ret;
}

int NvsIio::bufEnable(bool force)
{
    struct iioChannel *iCh;
    bool bufEn = false;
    int i;
    int bytes = 0;
    int ret = 0;

    /* calculate alignments and buffer size */
    if (mDevChN) {
        for (i = 0; i < mDevChN; i++) {
            /* don't enable buffer if just timestamp (handle = 0) */
            if (mDevCh[i].enabled && mDevCh[i].handle)
                bufEn = true;
            if (mDevCh[i].enabled) {
                iCh = (struct iioChannel *)mDevCh[i].drvCh;
                if (!(bytes % iCh->bytes))
                    iCh->location = bytes;
                else
                    iCh->location = bytes - bytes % iCh->bytes + iCh->bytes;
                bytes = iCh->location + iCh->bytes;
            }
        }
    }
    mBufSize = bytes;
    /* if something is enabled then enable buffer or just force on anyway */
    if (bufEn || force) {
        mLink = NULL; /* reset buffer fill flag */
        ret = sysfsIntWrite(mPathBufEnable, 1);
    } else {
        /* mPathEnable is for NVS and may not be supported hence no ret err */
        sysfsIntWrite(mPathEnable, 0); /* turn device power off */
    }
    if (ret)
        ALOGE("%s 1 -> %s ERR: %d\n", __func__, mPathBufEnable, ret);
    return ret;
}

int NvsIio::devChanAble(int32_t handle, int channel, int enable)
{
    struct iioChannel *iCh;
    bool err = true;
    int i;
    int ret;
    int ret_t;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s handle=%d channel=%d enable=%d\n",
             __func__, handle, channel, enable);
    if (!mDevChN) {
        ALOGE("%s ERR: -ENODEV\n", __func__);
        return -ENODEV;
    }

    /* disable buffer for any change */
    ret_t = bufDisable();
    if (ret_t)
        return ret_t;

    if ((handle > 0) && (channel < 0)) {
        /* enable/disable matching handle(s) */
        for (i = 0; i < mDevChN; i++) {
            if (mDevCh[i].handle == handle) {
                iCh = (struct iioChannel *)mDevCh[i].drvCh;
                err = false;
                ret = sysfsIntWrite(iCh->pathEn, enable);
                if (ret) {
                    ALOGE("%s %d -> %s ERR: %d\n",
                          __func__, enable, iCh->pathEn, ret);
                    ret_t |= ret;
                } else {
                    mDevCh[i].enabled = enable;
                }
            }
        }
        if (err) {
            ALOGE("%s ERR: handle not found\n", __func__);
            ret_t = -EINVAL;
        }
    } else if ((channel >= 0) && (channel < mDevChN)) {
        /* enable/disable single channel */
        iCh = (struct iioChannel *)mDevCh[channel].drvCh;
        ret_t = sysfsIntWrite(iCh->pathEn, enable);
        if (ret_t)
            ALOGE("%s %d -> %s ERR: %d\n",
                  __func__, enable, iCh->pathEn, ret_t);
        else
            mDevCh[channel].enabled = enable;
    } else {
        ALOGE("%s ERR: -EINVAL handle=%d  channel=%d  enable=%d\n",
              __func__, handle, channel, enable);
        ret_t = -EINVAL;
    }
    ret_t |= bufEnable(false);
    return ret_t;
}

int NvsIio::devGetChanN(const char *devName)
{
    struct iioChannel *iCh;
    int n;
    int i;

    ALOGI_IF(SensorBase::FUNC_ENTRY, "%s devName=%s\n", __func__, devName);
    if (mDevChN <= 0) {
        ALOGE("%s ERR: no device channels\n", __func__);
        return -EINVAL;
    }

    if (devName == NULL) {
        n = mDevChN;
        for (i = 0; i < mDevChN; i++) {
            iCh = (struct iioChannel *)mDevCh[i].drvCh;
            if (!(strcmp(strTimestamp, iCh->preName)))
                n--;
        }
        return n;
    }

    n = 0;
    for (i = 0; i < mDevChN; i++) {
        if (!(strcmp(devName, mDevCh[i].devName)))
            n++;
    }
    return n;
}

int NvsIio::devGetChanInfo(const char *devName,
                           struct devChannel **devCh)
{
    struct iioChannel *iCh;
    int n;
    int i;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s devName=%s devCh=%p\n", __func__, devName, devCh);
    if (!mDevChN) {
        ALOGE("%s ERR: no device channels\n", __func__);
        return -EINVAL;
    }

    if (devName == NULL) {
        n = 0;
        for (i = 0; i < mDevChN; i++) {
            iCh = (struct iioChannel *)mDevCh[i].drvCh;
            if (strcmp(strTimestamp, iCh->preName)) {
                devCh[n] = &mDevCh[i];
                n++;
            }
        }
        return n;
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

int NvsIio::devGetEventData(sensors_event_t* data, int count, int32_t handle)
{
    int n;
    int nEvents = 0;

    while (count) {
        if (mLink == NULL) {
            n = read(mFd, mBuf, mBufSize);
            if (SensorBase::ENG_VERBOSE &&
                ((mDbgFdN != n) || (mDbgBufSize != mBufSize))) {
                ALOGI("%s read %d bytes from fd %d (buffer size=%d)\n",
                      __func__, n, mFd, mBufSize);
                mDbgFdN = n;
                mDbgBufSize = mBufSize;
            }
            if (n < mBufSize)
                break;

            if (SensorBase::ENG_VERBOSE) {
                for (n = 0; n < mBufSize; n++)
                    ALOGI("%s buf byte %d=%x\n", __func__, n, mBuf[n]);
                if (mBufSize > (int)sizeof(__s64)) {
                    __s64 *ts = (__s64 *)mBuf;
                    ALOGI("%s timestamp=%lld (ts buffer index=%d)\n",
                          __func__, ts[(mBufSize / 8) - 1], mBufSize - 8);
                }
            }
            mLink = mLinkRoot;
        }
        n = mLink->processEvent(data, mBuf, handle);
        if (n > 0) {
            data++;
            nEvents++;
            count--;
        }
        mLink = mLink->getLink();
    }

    return nEvents;
}

int NvsIio::devGetChanData(struct devChannel *dCh, uint8_t *buf,
                           void *data, unsigned int dataSize)
{
    struct iioChannel *iCh = (struct iioChannel *)dCh->drvCh;
    uint64_t u64Data;

    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s devCh=%p buf=%p data=%p dataSize=%u\n",
             __func__, dCh, buf, data, dataSize);
    if (iCh->bytes > dataSize) {
        ALOGE("%s ERR: channel bytes=%u not supported\n",
              __func__, iCh->bytes);
        return -EINVAL;
    }

    u64Data = 0LL;
    memcpy(&u64Data, buf + iCh->location, iCh->bytes);
    u64Data >>= iCh->shift;
    u64Data &= iCh->mask;
    if (iCh->sign) {
        unsigned int shift = (sizeof(u64Data) * 8) - iCh->realbits;
        int64_t s64Data = (int64_t)u64Data;
        s64Data <<= shift;
        s64Data >>= shift;
        u64Data = s64Data;
    }
    memcpy(data, &u64Data, dataSize);
    return iCh->bytes;
}

int NvsIio::devGetTimestamp(struct devChannel *dCh,
                            uint8_t *buf, int64_t *ts)
{
    ALOGI_IF(SensorBase::FUNC_ENTRY,
             "%s dCh=%p buf=%p ts=%p\n", __func__, dCh, buf, ts);
    if (mDevChTs) {
        struct iioChannel *iCh = (struct iioChannel *)mDevChTs->drvCh;
        *ts = *((int64_t *)(buf + iCh->location));
        return iCh->bytes;
    }

    return -EINVAL;

}

void NvsIio::dbgChan()
{
    struct iioChannel *iCh;
    int i;
    int j;

    ALOGI("%s %s Number of channels=%d\n",
          __func__, mSysfsPath, mDevChN);
    for (i = 0; i < mDevChN; i++) {
        iCh = (struct iioChannel *)mDevCh[i].drvCh;
        ALOGI("------------------------------------\n");
        ALOGI("channel[%d].handle=%d\n", i, mDevCh[i].handle);
        ALOGI("channel[%d].devName=%s\n", i, mDevCh[i].devName);
        ALOGI("channel[%d].preName=%s\n", i, iCh->preName);
        ALOGI("channel[%d].postName=%s\n", i, iCh->postName);
        ALOGI("channel[%d].fullName=%s\n", i, iCh->fullName);
        ALOGI("channel[%d].pathEn=%s\n", i, iCh->pathEn);
        ALOGI("channel[%d].enabled=%u\n", i, mDevCh[i].enabled);
        ALOGI("channel[%d].dataType=%u\n", i, mDevCh[i].dataType);
        ALOGI("channel[%d].dataIndex=%u\n", i, mDevCh[i].dataIndex);
        ALOGI("channel[%d].index=%u\n", i, iCh->index);
        ALOGI("channel[%d].bigendian=%u\n", i, iCh->bigendian);
        ALOGI("channel[%d].sign=%u\n", i, iCh->sign);
        ALOGI("channel[%d].realbits=%u\n", i, iCh->realbits);
        ALOGI("channel[%d].shift=%u\n", i, iCh->shift);
        ALOGI("channel[%d].bytes=%u\n", i, iCh->bytes);
        ALOGI("channel[%d].mask=0x%llx\n", i, iCh->mask);
        ALOGI("channel[%d].location=%u\n", i, iCh->location);
        for (j = 0; j < ATTR_N; j++)
            ALOGI("channel[%d] %s path %s=%f (shared=%x)\n",
                  i, attrPathTbl[j].attrName, mDevCh[i].attrPath[j],
                  mDevCh[i].attrFVal[j], mDevCh[i].attrShared[j]);
    }
}

