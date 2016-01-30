/*
* adsp_console_dbfs.c
*
* adsp mailbox console driver
*
* Copyright (C) 2014, NVIDIA Corporation. All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/tegra_nvadsp.h>

#include <asm/uaccess.h>

#include "adsp_console_ioctl.h"
#include "adsp_console_dbfs.h"

static int open_cnt;

static int adsp_consol_open(struct inode *i, struct file *f)
{

	int ret;
	uint16_t snd_mbox_id = 30;
	struct nvadsp_cnsl *console = i->i_private;

	if (open_cnt)
		return -EBUSY;
	open_cnt++;
	ret = nvadsp_mbox_open(&console->shl_snd_mbox, &snd_mbox_id,
		"adsp_send_cnsl", NULL, NULL);
	if (ret)
		pr_err("adsp_consol: Failed to init adsp_consol send mailbox");

	f->private_data = console;

	return ret;
}
static int adsp_consol_close(struct inode *i, struct file *f)
{
	int ret;
	struct nvadsp_cnsl *console = i->i_private;
	struct nvadsp_mbox *mbox = &console->shl_snd_mbox;

	open_cnt--;

	ret = nvadsp_mbox_close(mbox);
	if (ret)
		pr_err("adsp_consol: Failed to close adsp_consol send mailbox)");

	memset(mbox, 0, sizeof(struct nvadsp_mbox));

	return ret;
}
static long
adsp_consol_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{

	int ret = 0;
	uint16_t *mid;
	uint16_t  mbxid;
	uint32_t  data;
	nvadsp_app_info_t *app_info;
	struct adsp_consol_run_app_arg_t app_args;
	struct nvadsp_cnsl *console = f->private_data;
	struct nvadsp_mbox *mbox;

	void __user *uarg = (void __user *)arg;

	if (_IOC_TYPE(cmd) != NV_ADSP_CONSOLE_MAGIC)
		return -EFAULT;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(ADSP_CNSL_LOAD):
		ret = nvadsp_os_load();
		if (ret)
			pr_info("adsp_consol: Failed to load OS.");
		else
			ret = nvadsp_os_start();
		if (ret)
			pr_info("adsp_consol: Failed to start OS");
		break;
	case _IOC_NR(ADSP_CNSL_RUN_APP):
		if (!access_ok(0, uarg,
			sizeof(struct adsp_consol_run_app_arg_t)))
			return -EACCES;
		ret = copy_from_user(&app_args, uarg,
			sizeof(app_args));
		if (ret) {
			ret = -EACCES;
			break;
		}
		app_args.ctx1 = (uint64_t)nvadsp_app_load(app_args.app_path,
			app_args.app_name);
		if (!app_args.ctx1) {
			pr_info("adsp_consol: dynamic app load failed ++++\n");
			return -1;
		}
		pr_info("adsp_consol: calling nvadsp_app_init\n");
		app_args.ctx2 =
			(uint64_t)nvadsp_app_init((void *)app_args.ctx1, NULL);
		if (!app_args.ctx2) {
			pr_info("adsp_consol: unable to initilize the app\n");
			return -1;
		}
		pr_info("adsp_consol: calling nvadsp_app_start\n");
		ret = nvadsp_app_start((void *)app_args.ctx2);
		if (ret) {
			pr_info("adsp_consol: unable to start the app\n");
			break;
		}
		ret = copy_to_user((void __user *) arg, &app_args,
			sizeof(struct adsp_consol_run_app_arg_t));
		if (ret)
			ret = -EACCES;
		break;
	case _IOC_NR(ADSP_CNSL_STOP_APP):
		if (!access_ok(0, uarg,
			sizeof(struct adsp_consol_run_app_arg_t)))
			return -EACCES;
		ret = copy_from_user(&app_args, uarg,
			sizeof(app_args));
		if (ret) {
			ret = -EACCES;
			break;
		}
		if ((!app_args.ctx2) || (!app_args.ctx1)) {
			ret = -EACCES;
			break;
		}
		nvadsp_app_deinit((void *)app_args.ctx2);
		nvadsp_app_unload((void *)app_args.ctx1);
		break;
	case _IOC_NR(ADSP_CNSL_CLR_BUFFER):
		break;
	case _IOC_NR(ADSP_CNSL_OPN_MBX):
		if (!access_ok(0, uarg,
			sizeof(struct adsp_consol_run_app_arg_t)))
			return -EACCES;
		ret = copy_from_user(&app_info, uarg,
			sizeof(app_info));
		if (ret) {
			ret = -EACCES;
			break;
		}
		mid = (short *)(app_info->mem.shared);
		pr_info("adsp_consol: open %x\n", *mid);
		mbxid = *mid;
		ret = nvadsp_mbox_open(&console->app_mbox, &mbxid,
			"app_mbox", NULL, NULL);
		if (ret) {
			pr_err("adsp_consol: Failed to open app mailbox");
			ret = -EACCES;
		}
		break;
	case _IOC_NR(ADSP_CNSL_CLOSE_MBX):
		mbox = &console->app_mbox;
		while (!nvadsp_mbox_recv(mbox, &data, 0, 0))
			;
		ret = nvadsp_mbox_close(mbox);
		if (ret)
			break;
		memset(mbox, 0, sizeof(struct nvadsp_mbox));
		break;
	case _IOC_NR(ADSP_CNSL_PUT_MBX):
		if (!access_ok(0, uarg,
			sizeof(struct adsp_consol_run_app_arg_t)))
			return -EACCES;
		ret = copy_from_user(&data, uarg,
			sizeof(uint32_t));
		if (ret) {
			ret = -EACCES;
			break;
		}
		ret = nvadsp_mbox_send(&console->app_mbox, data,
			NVADSP_MBOX_SMSG, 0, 0);
		break;
	case _IOC_NR(ADSP_CNSL_GET_MBX):
		if (!access_ok(0, uarg,
			sizeof(struct adsp_consol_run_app_arg_t)))
			return -EACCES;
		ret = nvadsp_mbox_recv(&console->app_mbox, &data, 0, 0);
		if (ret)
			break;
		ret = copy_to_user(uarg, &data,
			sizeof(uint32_t));
		if (ret)
			ret = -EACCES;
		break;
	case _IOC_NR(ADSP_CNSL_PUT_DATA):
		if (!access_ok(0, uarg,
			sizeof(struct adsp_consol_run_app_arg_t)))
			return -EACCES;
		ret = copy_from_user(&data, uarg, sizeof(uint32_t));
		if (ret) {
			ret = -EACCES;
			break;
		}
		return nvadsp_mbox_send(&console->shl_snd_mbox, data,
			NVADSP_MBOX_SMSG, 0, 0);
		break;
	default:
		pr_info("adsp_consol: invalid command\n");
		return -EINVAL;
	}
	return ret;
}

static const struct file_operations adsp_console_operations = {
	.open = adsp_consol_open,
	.release = adsp_consol_close,
#ifdef CONFIG_COMPAT
	.compat_ioctl = adsp_consol_ioctl,
#endif
	.unlocked_ioctl = adsp_consol_ioctl
};

int
adsp_create_cnsl(struct dentry *adsp_debugfs_root, struct nvadsp_cnsl *cnsl)
{
	int ret = 0;

	struct device *dev = cnsl->dev;

	if (IS_ERR_OR_NULL(adsp_debugfs_root)) {
		ret = -ENOENT;
		goto err_out;
	}

	if (!debugfs_create_file("adsp_console", S_IRUGO,
		adsp_debugfs_root, cnsl,
		&adsp_console_operations)) {
		dev_err(dev,
			"unable to create adsp console debug fs file\n");
		ret = -ENOENT;
		goto err_out;
	}

	memset(&cnsl->app_mbox, 0, sizeof(cnsl->app_mbox));

err_out:
	return ret;
}
