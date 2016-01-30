/*
 * drivers/video/tegra/host/nvhost_channel.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2014, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nvhost_channel.h"
#include "dev.h"
#include "nvhost_acm.h"
#include "nvhost_job.h"
#include "chip_support.h"

#include <trace/events/nvhost.h>
#include <linux/nvhost_ioctl.h>
#include <linux/delay.h>
#include <linux/nvhost.h>
#include <linux/slab.h>

#define NVHOST_CHANNEL_LOW_PRIO_MAX_WAIT 50

static struct mutex g_channel_lock;	/* global mutex for channels */

/* Memory allocation for all supported channels */
int nvhost_alloc_channels(struct nvhost_master *host)
{
	int max_channels = host->info.nb_channels;
	struct nvhost_channel *ch;
	int i, err;

	if (host->info.nb_channels > BITS_PER_LONG) {
		WARN(1, "host1x hardware has more channels than supported\n");
		return -ENOSYS;
	}

	host->chlist = kzalloc(host->info.nb_channels *
			       sizeof(struct nvhost_channel *), GFP_KERNEL);
	if (host->chlist == NULL)
		return -ENOMEM;

	mutex_init(&host->chlist_mutex);
	mutex_init(&g_channel_lock);

	for (i = 0; i < max_channels; i++) {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch) {
			dev_err(&host->dev->dev, "failed to alloc channels\n");
			return -ENOMEM;
		}

		/* initialize data structures */
		nvhost_set_chanops(ch);
		mutex_init(&ch->submitlock);
		mutex_init(&ch->syncpts_lock);
		ch->chid = i;

		/* initialize channel cdma */
		err = nvhost_cdma_init(host->dev, &ch->cdma);
		if (err) {
			dev_err(&host->dev->dev, "failed to initialize cdma\n");
			return err;
		}

		/* initialize hw specifics */
		err = channel_op(ch).init(ch, host);
		if (err < 0) {
			dev_err(&host->dev->dev, "failed to init channel %d\n",
				ch->chid);
			return err;
		}

		/* store the channel */
		host->chlist[i] = ch;
	}

	return 0;
}

/* return any one of assigned channel from device
 * This API can be used to check if any channel assigned to device
 */
struct nvhost_channel *nvhost_check_channel(struct nvhost_device_data *pdata)
{
	int i;
	struct nvhost_channel *ch;

	for (i = 0; i < pdata->num_channels; i++) {
		ch = pdata->channels[i];
		if (ch)
			return ch;
	}

	return NULL;
}

/* Check if more than channel needed for device and assign */
static int nvhost_channel_assign(struct nvhost_device_data *pdata,
			  struct nvhost_channel *ch)
{
	int i;

	for (i = 0; i < pdata->num_channels; i++) {
		if (!pdata->channels[i]) {
			pdata->channels[i] = ch;
			pdata->num_mapped_chs++;
			ch->dev_chid = i;
			return 0;
		}
	}
	dev_err(&pdata->pdev->dev, "%s: All channels assigned\n", __func__);

	return -EINVAL;
}

/* Releases all channels assigned with device */
int nvhost_channel_release(struct nvhost_device_data *pdata)
{
	struct nvhost_channel *ch;
	int i;

	for (i = 0; i < pdata->num_channels; i++) {
		ch = pdata->channels[i];
		if (ch && ch->dev)
			nvhost_putchannel(ch, 1);
	}
	return 0;
}
/* Unmap channel from device and free all resources, deinit device */
static int nvhost_channel_unmap_locked(struct nvhost_channel *ch)
{
	struct nvhost_device_data *pdata;
	struct nvhost_master *host;
	int max_channels;
	int i = 0;

	if (!ch->dev) {
		pr_err("%s: freeing unmapped channel\n", __func__);
		return 0;
	}

	pdata = platform_get_drvdata(ch->dev);
	host = nvhost_get_host(pdata->pdev);

	max_channels = host->info.nb_channels;

	/* turn off channel cdma */
	channel_cdma_op().stop(&ch->cdma);

	/* this is used only if we map channel on open */
	if (nvhost_get_channel_policy() == MAP_CHANNEL_ON_OPEN)
		pdata->num_mapped_chs--;

	/* log this event */
	dev_dbg(&ch->dev->dev, "channel %d un-mapped\n", ch->chid);
	trace_nvhost_channel_unmap_locked(pdata->pdev->name, ch->chid,
		pdata->num_mapped_chs);

	if (nvhost_get_syncpt_policy() == SYNCPT_PER_CHANNEL) {
		/* Release channel syncpoints */
		for (i = 0; i < NVHOST_MODULE_MAX_SYNCPTS; ++i) {
			if (ch->syncpts[i]) {
				nvhost_free_syncpt(ch->syncpts[i]);
				ch->syncpts[i] = 0;
			}
		}
	}

	clear_bit(ch->chid, &host->allocated_channels);

	ch->dev = NULL;
	ch->refcount = 0;
	ch->identifier = NULL;

	if (nvhost_get_channel_policy() == MAP_CHANNEL_ON_OPEN)
		pdata->channels[ch->dev_chid] = NULL;

	return 0;
}

/* Maps free channel with device */
int nvhost_channel_map(struct nvhost_device_data *pdata,
			struct nvhost_channel **channel,
			void *identifier)
{
	struct nvhost_master *host = NULL;
	struct nvhost_channel *ch = NULL;
	int max_channels = 0;
	int index = 0;

	if (!pdata) {
		pr_err("%s: NULL device data\n", __func__);
		return -EINVAL;
	}

	host = nvhost_get_host(pdata->pdev);

	mutex_lock(&host->chlist_mutex);
	max_channels = host->info.nb_channels;

	if (nvhost_get_channel_policy() == MAP_CHANNEL_ON_SUBMIT) {
		/* check if the channel is still in use */
		ch = *channel;
		if (ch && ch->refcount && ch->identifier == identifier) {
			/* yes, client can continue using it */
			ch->refcount++;
			mutex_unlock(&host->chlist_mutex);
			return 0;
		}
	} else if (pdata->num_channels == pdata->num_mapped_chs) {
		if (pdata->exclusive) {
			mutex_unlock(&host->chlist_mutex);
			return -EBUSY;
		}
		ch = nvhost_check_channel(pdata);
		if (ch)
			ch->refcount++;
		mutex_unlock(&host->chlist_mutex);
		*channel = ch;
		return 0;
	}

	do {
		index = find_first_zero_bit(&host->allocated_channels,
					    max_channels);
		if (index >= max_channels) {
			mutex_unlock(&host->chlist_mutex);
			if (nvhost_get_channel_policy() !=
				MAP_CHANNEL_ON_SUBMIT)
				return -ENOMEM;
			mdelay(1);
			mutex_lock(&host->chlist_mutex);
		}
	} while (index >= max_channels);

	/* Reserve the channel */
	set_bit(index, &host->allocated_channels);
	ch = host->chlist[index];

	/* Bind the reserved channel to the device */
	ch->dev = pdata->pdev;
	ch->identifier = identifier;
	if (nvhost_get_channel_policy() == MAP_CHANNEL_ON_OPEN)
		nvhost_channel_assign(pdata, ch);
	ch->refcount = 1;

	/* Handle logging */
	trace_nvhost_channel_map(pdata->pdev->name, ch->chid,
				 pdata->num_mapped_chs);
	dev_dbg(&ch->dev->dev, "channel %d mapped\n", ch->chid);

	mutex_unlock(&host->chlist_mutex);

	*channel = ch;
	return 0;
}

/* Free channel memory and list */
int nvhost_channel_list_free(struct nvhost_master *host)
{
	int i;

	for (i = 0; i < host->info.nb_channels; i++)
		kfree(host->chlist[i]);

	dev_info(&host->dev->dev, "channel list free'd\n");

	return 0;
}

void nvhost_channel_init_gather_filter(struct nvhost_channel *ch)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	if (channel_op(ch).init_gather_filter && pdata->gather_filter_enabled)
		channel_op(ch).init_gather_filter(ch);
}

int nvhost_channel_submit(struct nvhost_job *job)
{
	/*
	 * Check if queue has higher priority jobs running. If so, wait until
	 * queue is empty. Ignores result from nvhost_cdma_flush, as we submit
	 * either when push buffer is empty or when we reach the timeout.
	 */
	int higher_count = 0;

	switch (job->priority) {
	case NVHOST_PRIORITY_HIGH:
		higher_count = 0;
		break;
	case NVHOST_PRIORITY_MEDIUM:
		higher_count = job->ch->cdma.high_prio_count;
		break;
	case NVHOST_PRIORITY_LOW:
		higher_count = job->ch->cdma.high_prio_count
			+ job->ch->cdma.med_prio_count;
		break;
	}
	if (higher_count > 0)
		(void)nvhost_cdma_flush(&job->ch->cdma,
				NVHOST_CHANNEL_LOW_PRIO_MAX_WAIT);

	return channel_op(job->ch).submit(job);
}


void nvhost_getchannel(struct nvhost_channel *ch)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	struct nvhost_master *host = nvhost_get_host(pdata->pdev);

	mutex_lock(&host->chlist_mutex);
	ch->refcount++;
	trace_nvhost_getchannel(pdata->pdev->name, ch->refcount, ch->chid);
	mutex_unlock(&host->chlist_mutex);
}

void nvhost_putchannel(struct nvhost_channel *ch, int cnt)
{
	struct nvhost_device_data *pdata;
	struct nvhost_master *host;

	mutex_lock(&g_channel_lock);

	if (ch->dev == NULL) {
		pr_info("%s: ch->dev is null\n", __func__);
		mutex_unlock(&g_channel_lock);
		return;
	}
	pdata = platform_get_drvdata(ch->dev);
	host = nvhost_get_host(pdata->pdev);

	mutex_lock(&host->chlist_mutex);
	ch->refcount -= cnt;
	trace_nvhost_putchannel(pdata->pdev->name, ch->refcount, ch->chid);
	/* WARN on negative reference, with zero reference unmap channel*/
	if (!ch->refcount)
		nvhost_channel_unmap_locked(ch);
	else if (ch->refcount < 0)
		WARN_ON(1);
	mutex_unlock(&host->chlist_mutex);
	mutex_unlock(&g_channel_lock);
}

int nvhost_channel_suspend(struct nvhost_master *host)
{
	int i;

	for (i = 0; i < host->info.nb_channels; i++) {
		struct nvhost_channel *ch = host->chlist[i];
		if (channel_cdma_op().stop && ch->dev)
			channel_cdma_op().stop(&ch->cdma);
	}

	return 0;
}
