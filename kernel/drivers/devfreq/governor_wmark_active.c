/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>

#include <governor.h>

struct wmark_gov_info {
	/* probed from the devfreq */
	unsigned long		*freqlist;
	int			freq_count;

	/* algorithm parameters */
	unsigned int		p_load_target;
	unsigned int		p_load_max;

	/* common data */
	struct devfreq		*df;
	struct platform_device	*pdev;
	struct dentry		*debugdir;
};

static unsigned long freqlist_up(struct wmark_gov_info *wmarkinfo,
			unsigned long curr_freq)
{
	int i, pos;

	for (i = 0; i < wmarkinfo->freq_count; i++)
		if (wmarkinfo->freqlist[i] > curr_freq)
			break;

	pos = min(wmarkinfo->freq_count - 1, i);

	return wmarkinfo->freqlist[pos];
}

static unsigned long freqlist_down(struct wmark_gov_info *wmarkinfo,
			unsigned long curr_freq)
{
	int i, pos;

	for (i = wmarkinfo->freq_count - 1; i >= 0; i--)
		if (wmarkinfo->freqlist[i] < curr_freq)
			break;

	pos = max(0, i);
	return wmarkinfo->freqlist[pos];
}

static unsigned long freqlist_round(struct wmark_gov_info *wmarkinfo,
				    unsigned long freq)
{
	int i, pos;

	for (i = 0; i < wmarkinfo->freq_count; i++)
		if (wmarkinfo->freqlist[i] >= freq)
			break;

	pos = min(wmarkinfo->freq_count - 1, i);
	return wmarkinfo->freqlist[pos];
}

static void update_watermarks(struct devfreq *df,
			      unsigned long current_frequency)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	unsigned long relation = 0, next_freq = 0;
	unsigned long current_frequency_khz = current_frequency / 1000;

	if (current_frequency == wmarkinfo->freqlist[0]) {
		/* disable the low watermark if we are at lowest clock */
		df->profile->set_low_wmark(df->dev.parent, 0);
	} else {
		/* calculate the low threshold; what is the load value
		 * at which we would go into lower frequency given the
		 * that we are running at the new frequency? */
		next_freq = freqlist_down(wmarkinfo, current_frequency);
		relation = ((next_freq / current_frequency_khz) *
			wmarkinfo->p_load_target) / 1000;
		df->profile->set_low_wmark(df->dev.parent, relation);
	}

	if (current_frequency ==
	    wmarkinfo->freqlist[wmarkinfo->freq_count - 1]) {
		/* disable the high watermark if we are at highest clock */
		df->profile->set_high_wmark(df->dev.parent, 1000);
	} else {
		/* calculate the high threshold; what is the load value
		 * at which we would go into highest frequency given the
		 * that we are running at the new frequency? */
		next_freq = freqlist_up(wmarkinfo, current_frequency);
		relation = ((next_freq / current_frequency_khz) *
			wmarkinfo->p_load_target) / 1000;
		relation = min(wmarkinfo->p_load_max, relation);
		df->profile->set_high_wmark(df->dev.parent, relation);
	}

}

static int devfreq_watermark_target_freq(struct devfreq *df,
					 unsigned long *freq)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	struct devfreq_dev_status dev_stat;
	unsigned long long load, relation, next_freq;
	int err;

	err = df->profile->get_dev_status(df->dev.parent, &dev_stat);
	if (err < 0)
		return err;

	/* keep current frequency if we do not have proper data available */
	if (!dev_stat.total_time) {
		*freq = dev_stat.current_frequency;
		return 0;
	}

	/* calculate first load and relation load/p_load_target */
	load = (dev_stat.busy_time * 1000) / dev_stat.total_time;

	/* if we cross load max...  */
	if (load >= wmarkinfo->p_load_max) {
		/* we go directly to the highest frequency. depending
		 * on frequency table we might never go higher than
		 * the current frequency (i.e. load should be over 100%
		 * to make relation push to the next frequency). */
		*freq = wmarkinfo->freqlist[wmarkinfo->freq_count - 1];
	} else {
		/* otherwise, based on relation between current load and
		 * load target we calculate the "ideal" frequency
		 * where we would be just at the target */
		relation = (load * 1000) / wmarkinfo->p_load_target;
		next_freq = relation * (dev_stat.current_frequency / 1000);

		/* round this frequency */
		*freq = freqlist_round(wmarkinfo, next_freq);
	}

	/* update watermarks to match with the new frequency */
	update_watermarks(df, *freq);

	return 0;
}

static void devfreq_watermark_debug_start(struct devfreq *df)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	struct dentry *f;
	char dirname[128];

	snprintf(dirname, sizeof(dirname), "%s_scaling",
		to_platform_device(df->dev.parent)->name);

	if (!wmarkinfo)
		return;

	wmarkinfo->debugdir = debugfs_create_dir(dirname, NULL);
	if (!wmarkinfo->debugdir) {
		pr_warn("cannot create debugfs directory\n");
		return;
	}

#define CREATE_DBG_FILE(fname) \
	do {\
		f = debugfs_create_u32(#fname, S_IRUGO | S_IWUSR, \
			wmarkinfo->debugdir, &wmarkinfo->p_##fname); \
		if (NULL == f) { \
			pr_warn("cannot create debug entry " #fname "\n"); \
			return; \
		} \
	} while (0)

	CREATE_DBG_FILE(load_target);
	CREATE_DBG_FILE(load_max);
#undef CREATE_DBG_FILE

}

static void devfreq_watermark_debug_stop(struct devfreq *df)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	debugfs_remove_recursive(wmarkinfo->debugdir);
}

static int devfreq_watermark_start(struct devfreq *df)
{
	struct wmark_gov_info *wmarkinfo;
	struct platform_device *pdev = to_platform_device(df->dev.parent);

	if (!df->profile->freq_table) {
		dev_err(&pdev->dev, "Frequency table missing\n");
		return -EINVAL;
	}

	wmarkinfo = kzalloc(sizeof(struct wmark_gov_info), GFP_KERNEL);
	if (!wmarkinfo)
		return -ENOMEM;

	df->data = (void *)wmarkinfo;
	wmarkinfo->freqlist = df->profile->freq_table;
	wmarkinfo->freq_count = df->profile->max_state;
	wmarkinfo->p_load_target = 800;
	wmarkinfo->p_load_max = 950;
	wmarkinfo->df = df;
	wmarkinfo->pdev = pdev;

	devfreq_watermark_debug_start(df);

	return 0;
}

static int devfreq_watermark_event_handler(struct devfreq *df,
			unsigned int event, void *wmark_type)
{
	int ret = 0;
	struct wmark_gov_info *wmarkinfo = df->data;

	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_watermark_start(df);
		wmarkinfo = df->data;
		update_watermarks(df, wmarkinfo->freqlist[0]);
		break;
	case DEVFREQ_GOV_STOP:
		devfreq_watermark_debug_stop(df);
		break;
	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(df);
		break;

	case DEVFREQ_GOV_RESUME:
		wmarkinfo = df->data;
		update_watermarks(df, wmarkinfo->freqlist[0]);
		devfreq_monitor_resume(df);
		break;

	case DEVFREQ_GOV_WMARK:
		mutex_lock(&df->lock);
		update_devfreq(df);
		mutex_unlock(&df->lock);
		break;

	default:
		break;
	}

	return ret;
}

static struct devfreq_governor devfreq_watermark_active = {
	.name = "wmark_active",
	.get_target_freq = devfreq_watermark_target_freq,
	.event_handler = devfreq_watermark_event_handler,
};


static int __init devfreq_watermark_init(void)
{
	return devfreq_add_governor(&devfreq_watermark_active);
}

static void __exit devfreq_watermark_exit(void)
{
	devfreq_remove_governor(&devfreq_watermark_active);
}

rootfs_initcall(devfreq_watermark_init);
module_exit(devfreq_watermark_exit);
