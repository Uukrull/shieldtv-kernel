/*
 * drivers/platform/tegra/tegra21_dvfs.c
 *
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/tegra-fuse.h>
#include <linux/pm_qos.h>
#include <linux/of_address.h>

#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/tegra_cl_dvfs.h>
#include <linux/platform/tegra/cpu-tegra.h>
#include "board.h"
#include "tegra_core_sysfs_limits.h"

static bool tegra_dvfs_cpu_disabled;
static bool tegra_dvfs_core_disabled;
static bool tegra_dvfs_gpu_disabled;

#define KHZ 1000
#define MHZ 1000000

/* FIXME: need tegra21 step */
#define VDD_SAFE_STEP			100

static int vdd_core_vmin_trips_table[MAX_THERMAL_LIMITS];
static int vdd_core_therm_floors_table[MAX_THERMAL_LIMITS];
static struct tegra_cooling_device core_vmin_cdev = {
	.compatible = "nvidia,tegra210-rail-vmin-cdev",
};

static int vdd_cpu_vmin_trips_table[MAX_THERMAL_LIMITS];
static int vdd_cpu_therm_floors_table[MAX_THERMAL_LIMITS];
static struct tegra_cooling_device cpu_vmin_cdev = {
	.compatible = "nvidia,tegra210-rail-vmin-cdev",
};
static int vdd_cpu_vmax_trips_table[MAX_THERMAL_LIMITS];
static int vdd_cpu_therm_caps_table[MAX_THERMAL_LIMITS];
#ifndef CONFIG_TEGRA_CPU_VOLT_CAP
static struct tegra_cooling_device cpu_vmax_cdev = {
	.compatible = "nvidia,tegra210-rail-vmax-cdev",
};
#endif

static struct tegra_cooling_device gpu_vts_cdev = {
	.compatible = "nvidia,tegra210-rail-scaling-cdev",
};

static struct dvfs_rail tegra21_dvfs_rail_vdd_cpu = {
	.reg_id = "vdd_cpu",
	.max_millivolts = 1300,
	.step = VDD_SAFE_STEP,
	.jmp_to_zero = true,
	.vmin_cdev = &cpu_vmin_cdev,
#ifndef CONFIG_TEGRA_CPU_VOLT_CAP
	.vmax_cdev = &cpu_vmax_cdev,
#endif
	.alignment = {
		.step_uv = 6250, /* 6.25mV */
	},
	.stats = {
		.bin_uV = 6250, /* 6.25mV */
	},
	.version = "p4v30",
};

static struct dvfs_rail tegra21_dvfs_rail_vdd_core = {
	.reg_id = "vdd_core",
	.max_millivolts = 1300,
	.min_millivolts = 825,
	.step = VDD_SAFE_STEP,
	.step_up = 1300,
	.vmin_cdev = &core_vmin_cdev,
	.alignment = {
		.step_uv = 12500, /* 12.5mV */
	},
	.version = "p4v23",
};

static struct dvfs_rail tegra21_dvfs_rail_vdd_gpu = {
	.reg_id = "vdd_gpu",
	.max_millivolts = 1300,
	.step = VDD_SAFE_STEP,
	.step_up = 1300,
	.in_band_pm = true,
	.vts_cdev = &gpu_vts_cdev,
	.alignment = {
		.step_uv = 6250, /* 6.25mV */
	},
	.stats = {
		.bin_uV = 6250, /* 6.25mV */
	},
	.version = "p4v26",
};

static struct dvfs_rail *tegra21_dvfs_rails[] = {
	&tegra21_dvfs_rail_vdd_cpu,
	&tegra21_dvfs_rail_vdd_core,
	&tegra21_dvfs_rail_vdd_gpu,
};

void __init tegra21x_vdd_cpu_align(int step_uv, int offset_uv)
{
	tegra21_dvfs_rail_vdd_cpu.alignment.step_uv = step_uv;
	tegra21_dvfs_rail_vdd_cpu.alignment.offset_uv = offset_uv;
}

/* FIXME: Remove after bringup */
#define BRINGUP_CVB_V_MARGIN	25
#define BRINGUP_CVB_V_MARGIN_EX	5

/* CPU DVFS tables */
static unsigned long cpu_max_freq[] = {
/* speedo_id	0	 1        2      */
		1912500, 1912500, 2218500,
};

#define CPU_CVB_TABLE		\
	.freqs_mult = KHZ,	\
	.speedo_scale = 100,	\
	.voltage_scale = 1000,	\
	.cvb_table = {		\
		/* f	  dfll:    c0,       c1,       c2    pll:    c0,       c1,       c2 */  \
		{  204000, {     1607,    80055,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  306000, {    39154,    78855,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  408000, {    78621,    77665,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  510000, {   120010,    76475,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  612000, {   163319,    75285,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  714000, {   208550,    74085,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  816000, {   255701,    72895,    -2323 }, {  0       ,  0      ,  0     } }, \
		{  918000, {   304773,    71705,    -2323 }, {  0       ,  0      ,  0     } }, \
		{ 1020000, {   355766,    70515,    -2323 }, { -2875621 ,  358099 , -8585  } }, \
		{ 1122000, {   408680,    69315,    -2323 }, { -52225   ,  104159 , -2816  } }, \
		{ 1224000, {   463515,    68125,    -2323 }, {  1076868 ,  8356   , -727   } }, \
		{ 1326000, {   520271,    66935,    -2323 }, {  2208191 , -84659  ,  1240  } }, \
		{ 1428000, {   578948,    65745,    -2323 }, {  2519460 , -105063 ,  1611  } }, \
		{ 1530000, {   639546,    64545,    -2323 }, {  2639809 , -108729 ,  1626  } }, \
		{ 1632000, {   702064,    63355,    -2323 }, {  2889664 , -122173 ,  1834  } }, \
		{ 1734000, {   766504,    62165,    -2323 }, {  3386160 , -154021 ,  2393  } }, \
		{ 1836000, {   832865,    60975,    -2323 }, {  5100873 , -279186 ,  4747  } }, \
		{ 1912500, {   863559,    60085,    -2323 }, {  5100873 , -279186 ,  4747  } }, \
		{ 2014500, {   939271,    58895,    -2323 }, {  5100873 , -279186 ,  4747  } }, \
		{ 2218500, {   1227000,       0,        0 }, {  5100873 , -279186 ,  4747  } }, \
		{ 0,	   { }, { }, }, \
	}

static struct cpu_cvb_dvfs cpu_cvb_dvfs_table[] = {
	{
		.speedo_id = 2,
		.process_id = 0,
		.dfll_tune_data  = {
			.tune0		= 0xFFEAD0FF,
			.tune1		= 0x020091D9,
			.droop_rate_min = 1000000,
			.min_millivolts = 870,
		},
		.pll_tune_data = {
			.min_millivolts = 950,
		},
		.max_mv = 1227,
		CPU_CVB_TABLE,
	},
	{
		.speedo_id = 2,
		.process_id = 1,
		.dfll_tune_data  = {
			.tune0		= 0xFFEAD0FF,
			.tune1		= 0x025501D0,
			.droop_rate_min = 1000000,
			.min_millivolts = 870,
		},
		.pll_tune_data = {
			.min_millivolts = 950,
		},
		.max_mv = 1227,
		CPU_CVB_TABLE,
	},
	{
		.speedo_id = -1,
		.process_id = 0,
		.dfll_tune_data  = {
			.tune0		= 0xFFEAD0FF,
			.tune1		= 0x020091D9,
			.droop_rate_min = 1000000,
			.min_millivolts = 850,
		},
		.pll_tune_data = {
			.min_millivolts = 950,
		},
		.max_mv = 1170,
		CPU_CVB_TABLE,
	},
	{
		.speedo_id = -1,
		.process_id = 1,
		.dfll_tune_data  = {
			.tune0		= 0xFFEAD0FF,
			.tune1		= 0x025501D0,
			.droop_rate_min = 1000000,
			.min_millivolts = 850,
		},
		.pll_tune_data = {
			.min_millivolts = 950,
		},
		.max_mv = 1170,
		CPU_CVB_TABLE,
	},
};

static int cpu_millivolts[MAX_DVFS_FREQS];
static int cpu_dfll_millivolts[MAX_DVFS_FREQS];

static struct dvfs cpu_dvfs = {
	.clk_name	= "cpu_g",
	.millivolts	= cpu_millivolts,
	.dfll_millivolts = cpu_dfll_millivolts,
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra21_dvfs_rail_vdd_cpu,
};

/* CPU LP DVFS tables */
static unsigned long cpu_lp_max_freq[] = {
/* speedo_id	0	 1        2 */
		1228800, 1228800, 1228800,
};

#define CPU_LP_CVB_TABLE	\
	.freqs_mult = KHZ,	\
	.speedo_scale = 100,	\
	.voltage_scale = 1000,	\
	.cvb_table = {		\
		/* f	   dfll pll:    c0,       c1,       c2 */    \
		{  204000, { }, {  1112126,   -14875,     -200 }, }, \
		{  307200, { }, {  1155822,   -15435,     -200 }, }, \
		{  403200, { }, {  1202425,   -16005,     -200 }, }, \
		{  518400, { }, {  1251935,   -16565,     -200 }, }, \
		{  614400, { }, {  1304351,   -17125,     -200 }, }, \
		{  710400, { }, {  1359673,   -17685,     -200 }, }, \
		{  825600, { }, {  1417902,   -18245,     -200 }, }, \
		{  921600, { }, {  1479037,   -18815,     -200 }, }, \
		{ 1036800, { }, {  1543079,   -19375,     -200 }, }, \
		{ 1132800, { }, {  1610027,   -19935,     -200 }, }, \
		{ 1228800, { }, {  1675881,   -20495,     -200 }, }, \
		{ 0,	   { }, { }, }, \
	}

static struct cpu_cvb_dvfs cpu_lp_cvb_dvfs_table[] = {
	{
		.speedo_id = -1,
		.process_id = -1,
		.pll_tune_data = {
			.min_millivolts = 850,
		},
		.max_mv = 1170,
		CPU_LP_CVB_TABLE,
	},
};

static int cpu_lp_millivolts[MAX_DVFS_FREQS];

static struct dvfs cpu_lp_dvfs = {
	.clk_name	= "cpu_lp",
	.millivolts	= cpu_lp_millivolts,
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra21_dvfs_rail_vdd_cpu,
};

/* GPU DVFS tables */
static unsigned long gpu_max_freq[] = {
/* speedo_id	0	 1      */
		921600,  998400,
};

#define NA_FREQ_CVB_TABLE	\
	.freqs_mult = KHZ,	\
	.speedo_scale = 100,	\
	.thermal_scale = 10,	\
	.voltage_scale = 1000,	\
	.cvb_table = {		\
		/* f	   dfll pll:    c0,       c1,       c2,       c3,       c4,       c5 */    \
		{   76800, { }, {   814294,     8144,     -940,      808,   -21583,      226 }, }, \
		{  153600, { }, {   856185,     8144,     -940,      808,   -21583,      226 }, }, \
		{  230400, { }, {   898077,     8144,     -940,      808,   -21583,      226 }, }, \
		{  307200, { }, {   939968,     8144,     -940,      808,   -21583,      226 }, }, \
		{  384000, { }, {   981860,     8144,     -940,      808,   -21583,      226 }, }, \
		{  460800, { }, {  1023751,     8144,     -940,      808,   -21583,      226 }, }, \
		{  537600, { }, {  1065642,     8144,     -940,      808,   -21583,      226 }, }, \
		{  614400, { }, {  1107534,     8144,     -940,      808,   -21583,      226 }, }, \
		{  691200, { }, {  1149425,     8144,     -940,      808,   -21583,      226 }, }, \
		{  768000, { }, {  1191317,     8144,     -940,      808,   -21583,      226 }, }, \
		{  844800, { }, {  1233208,     8144,     -940,      808,   -21583,      226 }, }, \
		{  921600, { }, {  1275100,     8144,     -940,      808,   -21583,      226 }, }, \
		{  998400, { }, {  1316991,     8144,     -940,      808,   -21583,      226 }, }, \
		{ 0,	   { }, { }, }, \
	}, \
	.cvb_vmin = {   0, { }, {   840000,        0,        0 }, }

#define FIXED_FREQ_CVB_TABLE	\
	.freqs_mult = KHZ,	\
	.speedo_scale = 100,	\
	.thermal_scale = 10,	\
	.voltage_scale = 1000,	\
	.cvb_table = {		\
		/* f	   dfll pll:    c0,       c1,       c2 */    \
		{   76800, { }, {  1786666,   -85625,     1632 }, }, \
		{  153600, { }, {  1846729,   -87525,     1632 }, }, \
		{  230400, { }, {  1910480,   -89425,     1632 }, }, \
		{  307200, { }, {  1977920,   -91325,     1632 }, }, \
		{  384000, { }, {  2049049,   -93215,     1632 }, }, \
		{  460800, { }, {  2122872,   -95095,     1632 }, }, \
		{  537600, { }, {  2201331,   -96985,     1632 }, }, \
		{  614400, { }, {  2283479,   -98885,     1632 }, }, \
		{  691200, { }, {  2369315,  -100785,     1632 }, }, \
		{  768000, { }, {  2458841,  -102685,     1632 }, }, \
		{  844800, { }, {  2550821,  -104555,     1632 }, }, \
		{  921600, { }, {  2647676,  -106455,     1632 }, }, \
		{ 0,	   { }, { }, }, \
	}, \
	.cvb_vmin = {   0, { }, {   950000,        0,        0 }, }

static struct gpu_cvb_dvfs gpu_cvb_dvfs_table[] = {
	{
		.speedo_id = 0,
		.process_id = -1,
#ifdef CONFIG_TEGRA_GPU_DVFS
		.max_mv = 1150,
#else
		.max_mv = 1000,
#endif
		FIXED_FREQ_CVB_TABLE,
	},
	{
		.speedo_id = 1,
		.process_id = -1,
		.max_mv = 1150,
#ifdef CONFIG_TEGRA_USE_NA_GPCPLL
		NA_FREQ_CVB_TABLE,
#else
		FIXED_FREQ_CVB_TABLE,
#endif
	},
};

static int gpu_vmin[MAX_THERMAL_RANGES];
static int gpu_peak_millivolts[MAX_DVFS_FREQS];
static int gpu_millivolts[MAX_THERMAL_RANGES][MAX_DVFS_FREQS];
static struct dvfs gpu_dvfs = {
	.clk_name	= "gbus",
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra21_dvfs_rail_vdd_gpu,
};

/* Core DVFS tables */
static const int core_millivolts[MAX_DVFS_FREQS] = {
	825, 850, 875, 900, 925, 950, 975, 1000, 1025, 1050, 1075, 1100, 1125 };

#define CORE_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...) \
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.dvfs_rail	= &tegra21_dvfs_rail_vdd_core,	\
	}

#define OVRRD_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...) \
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.can_override	= true,				\
		.dvfs_rail	= &tegra21_dvfs_rail_vdd_core,	\
	}

/*
 * Exceptions in the below table that disable framework auto-dvfs:
 *
 * sbus: Tegra21 platform code calls tegra_dvfs_set_rate manually;
 * it is necessary for special handling of system clock skipper enabled on T210
 *
 * display: display driver calls tegra_dvfs_set_rate manually;
 * it is necessary because display clock divider is internal to the display
 * block, and controlled by driver directly.
 */
static struct dvfs core_dvfs_table[] = {
	/* Clock limits for internal blocks, PLLs */
	/* Core voltages(mV):				    825,     850,     875,     900,     925,     950,     975,    1000,    1025,    1050,    1075,    1100,    1125 */
	CORE_DVFS("emc",		-1, 0, 1, KHZ,	      1,       1,       1,       1,       1,       1,       1, 1200000, 1200000, 1200000, 1200000, 1200000, 1800000),

	CORE_DVFS("vic03",		-1, 0, 1, KHZ,	 140800,  268800,  332800,  371200,  409600,  435200,  473600,  499200,  537600,  563200,  588800,  601600,  627200),
	CORE_DVFS("nvjpg",		-1, 0, 1, KHZ,	 140800,  268800,  332800,  371200,  409600,  435200,  473600,  499200,  537600,  563200,  588800,  601600,  627200),
	CORE_DVFS("se",			-1, 0, 1, KHZ,	 140800,  268800,  332800,  371200,  409600,  435200,  473600,  499200,  537600,  563200,  588800,  601600,  627200),
	CORE_DVFS("tsecb",		-1, 0, 1, KHZ,	 140800,  268800,  332800,  371200,  409600,  435200,  473600,  499200,  537600,  563200,  588800,  601600,  627200),
	CORE_DVFS("c2bus",		-1, 0, 1, KHZ,	 140800,  268800,  332800,  371200,  409600,  435200,  473600,  499200,  537600,  563200,  588800,  601600,  627200),

	CORE_DVFS("msenc",		-1, 0, 1, KHZ,	 192000,  345600,  396800,  435200,  473600,  512000,  563200,  601600,  627200,  652800,  678400,  691200,  716800),
	CORE_DVFS("nvdec",		-1, 0, 1, KHZ,	 192000,  345600,  396800,  435200,  473600,  512000,  563200,  601600,  627200,  652800,  678400,  691200,  716800),
	CORE_DVFS("c3bus",		-1, 0, 1, KHZ,	 192000,  345600,  396800,  435200,  473600,  512000,  563200,  601600,  627200,  652800,  678400,  691200,  716800),

	CORE_DVFS("vi",			-1, 0, 1, KHZ,	      1,       1,  307200,  371200,  435200,  499200,  550400,  614400,  678400,  742400,  793600,  793600,  793600),
	CORE_DVFS("isp",		-1, 0, 1, KHZ,	      1,       1,  307200,  371200,  435200,  499200,  550400,  614400,  678400,  742400,  793600,  793600,  793600),
	CORE_DVFS("cbus",		-1, 0, 1, KHZ,	      1,       1,  307200,  371200,  435200,  499200,  550400,  614400,  678400,  742400,  793600,  793600,  793600),

	CORE_DVFS("adsp_cpu",		-1, 0, 1, KHZ,	 153600,  332800,  371200,  422400,  486400,  563200,  614400,  691200,  742400,  780800,  819200,  844800,  844800),
	CORE_DVFS("ape",		-1, 0, 1, KHZ,	 140800,  230400,  268800,  307200,  345600,  384000,  448000,  486400,  499200,  499200,  499200,  499200,  499200),

	CORE_DVFS("sbus",		-1, 0, 0, KHZ,	 115200,  179200,  217600,  243200,  268800,  294400,  320000,  345600,  358400,  371200,  384000,  408000,  408000),
	CORE_DVFS("host1x",		-1, 0, 1, KHZ,	  81600,  140800,  153600,  166400,  192000,  230400,  281600,  320000,  345600,  371200,  384000,  408000,  408000),
	CORE_DVFS("tsec",		-1, 0, 1, KHZ,	 217600,  384000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("mselect",		-1, 0, 1, KHZ,	 204000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),

	CORE_DVFS("disp1",		-1, 0, 0, KHZ,	 153600,  332800,  371200,  409600,  422400,  460800,  499200,  537600,  576000,  601600,  640000,  665600,  665600),
	CORE_DVFS("disp2",		-1, 0, 0, KHZ,	 153600,  332800,  371200,  409600,  422400,  460800,  499200,  537600,  576000,  601600,  640000,  665600,  665600),

	/* Core voltages(mV):				    825,     850,     875,     900,     925,     950,     975,    1000,    1025,    1050,    1075,    1100,    1125 */
	CORE_DVFS("emc",		-1, 1, 1, KHZ,	      1,       1,       1,       1,       1,       1,       1, 1200000, 1200000, 1200000, 1800000, 1800000, 1800000),

	CORE_DVFS("vic03",		-1, 1, 1, KHZ,	 192000,  307200,  345600,  409600,  486400,  524800,  550400,  576000,  588800,  614400,  627200,  627200,  627200),
	CORE_DVFS("nvjpg",		-1, 1, 1, KHZ,	 192000,  307200,  345600,  409600,  486400,  524800,  550400,  576000,  588800,  614400,  627200,  627200,  627200),
	CORE_DVFS("se",			-1, 1, 1, KHZ,	 192000,  307200,  345600,  409600,  486400,  524800,  550400,  576000,  588800,  614400,  627200,  627200,  627200),
	CORE_DVFS("tsecb",		-1, 1, 1, KHZ,	 192000,  307200,  345600,  409600,  486400,  524800,  550400,  576000,  588800,  614400,  627200,  627200,  627200),
	CORE_DVFS("c2bus",		-1, 1, 1, KHZ,	 192000,  307200,  345600,  409600,  486400,  524800,  550400,  576000,  588800,  614400,  627200,  627200,  627200),

	CORE_DVFS("msenc",		-1, 1, 1, KHZ,	 268800,  384000,  448000,  486400,  550400,  576000,  614400,  652800,  678400,  691200,  716800,  716800,  716800),
	CORE_DVFS("nvdec",		-1, 1, 1, KHZ,	 268800,  384000,  448000,  486400,  550400,  576000,  614400,  652800,  678400,  691200,  716800,  716800,  716800),
	CORE_DVFS("c3bus",		-1, 1, 1, KHZ,	 268800,  384000,  448000,  486400,  550400,  576000,  614400,  652800,  678400,  691200,  716800,  716800,  716800),

	CORE_DVFS("vi",			-1, 1, 1, KHZ,	      1,       1,  473600,  576000,  588800,  678400,  691200,  691200,  691200,  793600,  793600,  793600,  793600),
	CORE_DVFS("isp",		-1, 1, 1, KHZ,	      1,       1,  473600,  576000,  588800,  678400,  691200,  691200,  691200,  793600,  793600,  793600,  793600),
	CORE_DVFS("cbus",		-1, 1, 1, KHZ,	      1,       1,  473600,  576000,  588800,  678400,  691200,  691200,  691200,  793600,  793600,  793600,  793600),

	CORE_DVFS("adsp_cpu",		-1, 1, 1, KHZ,	 230400,  422400,  460800,  524800,  601600,  652800,  704000,  755200,  819200,  844800,  844800,  844800,  844800),
	CORE_DVFS("ape",		-1, 1, 1, KHZ,	 179200,  307200,  345600,  371200,  409600,  422400,  460800,  499200,  499200,  499200,  499200,  499200,  499200),

	CORE_DVFS("sbus",		-1, 1, 0, KHZ,	 140800,  230400,  256000,  281600,  307200,  332800,  358400,  371200,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("host1x",		-1, 1, 1, KHZ,	  81600,  153600,  179200,  192000,  217600,  345600,  358400,  384000,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("tsec",		-1, 1, 1, KHZ,	 268800,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("mselect",		-1, 1, 1, KHZ,	 204000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),

	CORE_DVFS("disp1",		-1, 1, 0, KHZ,	 243200,  422400,  460800,  499200,  524800,  563200,  576000,  601600,  640000,  665600,  665600,  665600,  665600),
	CORE_DVFS("disp2",		-1, 1, 0, KHZ,	 243200,  422400,  460800,  499200,  524800,  563200,  576000,  601600,  640000,  665600,  665600,  665600,  665600),

	/* Core voltages(mV):				    825,     850,     875,     900,     925,     950,     975,    1000,    1025,    1050,    1075,    1100,    1125 */
	CORE_DVFS("emc",		-1, 2, 1, KHZ,	      1,       1,       1,       1,       1,       1,       1, 1200000, 1200000, 1200000, 1200000, 1200000, 1200000),

	CORE_DVFS("vic03",		-1, 2, 1, KHZ,	 230400,  371200,  448000,  499200,  563200,  627200,  627200,  627200,  627200,  627200,  627200,  627200,  627200),
	CORE_DVFS("nvjpg",		-1, 2, 1, KHZ,	 230400,  371200,  448000,  499200,  563200,  627200,  627200,  627200,  627200,  627200,  627200,  627200,  627200),
	CORE_DVFS("se",			-1, 2, 1, KHZ,	 230400,  371200,  448000,  499200,  563200,  627200,  627200,  627200,  627200,  627200,  627200,  627200,  627200),
	CORE_DVFS("tsecb",		-1, 2, 1, KHZ,	 230400,  371200,  448000,  499200,  563200,  627200,  627200,  627200,  627200,  627200,  627200,  627200,  627200),
	CORE_DVFS("c2bus",		-1, 2, 1, KHZ,	 230400,  371200,  448000,  499200,  563200,  627200,  627200,  627200,  627200,  627200,  627200,  627200,  627200),

	CORE_DVFS("msenc",		-1, 2, 1, KHZ,	 307200,  486400,  563200,  614400,  640000,  652800,  678400,  716800,  716800,  716800,  716800,  716800,  716800),
	CORE_DVFS("nvdec",		-1, 2, 1, KHZ,	 307200,  486400,  563200,  614400,  640000,  652800,  678400,  716800,  716800,  716800,  716800,  716800,  716800),
	CORE_DVFS("c3bus",		-1, 2, 1, KHZ,	 307200,  486400,  563200,  614400,  640000,  652800,  678400,  716800,  716800,  716800,  716800,  716800,  716800),

	CORE_DVFS("vi",			-1, 2, 1, KHZ,	      1,  588800,  678400,  691200,  691200,  768000,  793600,  793600,  793600,  793600,  793600,  793600,  793600),
	CORE_DVFS("isp",		-1, 2, 1, KHZ,	      1,  588800,  678400,  691200,  691200,  768000,  793600,  793600,  793600,  793600,  793600,  793600,  793600),
	CORE_DVFS("cbus",		-1, 2, 1, KHZ,	      1,  588800,  678400,  691200,  691200,  768000,  793600,  793600,  793600,  793600,  793600,  793600,  793600),

	CORE_DVFS("adsp_cpu",		-1, 2, 1, KHZ,	 281600,  499200,  576000,  652800,  691200,  755200,  793600,  844800,  844800,  844800,  844800,  844800,  844800),
	CORE_DVFS("ape",		-1, 2, 1, KHZ,	 230400,  358400,  396800,  422400,  486400,  524800,  537600,  563200,  563200,  563200,  563200,  563200,  563200),

	CORE_DVFS("sbus",		-1, 2, 0, KHZ,	 204800,  307200,  332800,  371200,  384000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("host1x",		-1, 2, 1, KHZ,	 128000,  217600,  345600,  384000,  384000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("tsec",		-1, 2, 1, KHZ,	 345600,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),
	CORE_DVFS("mselect",		-1, 2, 1, KHZ,	 204000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000,  408000),

	CORE_DVFS("disp1",		-1, 2, 0, KHZ,	 345600,  486400,  524800,  563200,  576000,  627200,  640000,  665600,  665600,  665600,  665600,  665600,  665600),
	CORE_DVFS("disp2",		-1, 2, 0, KHZ,	 345600,  486400,  524800,  563200,  576000,  627200,  640000,  665600,  665600,  665600,  665600,  665600,  665600),

	/* Core voltages(mV):				    825,     850,     875,     900,     925,     950,     975,    1000,    1025,    1050,    1075,    1100,    1125 */
	CORE_DVFS("pll_c",		-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),
	CORE_DVFS("pll_c2",		-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),
	CORE_DVFS("pll_c3",		-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),
	CORE_DVFS("pll_c4_out0",	-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),
	CORE_DVFS("pll_d_out0",		-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),
	CORE_DVFS("pll_d2",		-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),
	CORE_DVFS("pll_dp",		-1, -1, 1, KHZ,	1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400, 1190400),

	/* Clock limits for I/O peripherals */
	/* Core voltages(mV):				    825,     850,     875,     900,     925,     950,     975,    1000,    1025,    1050,    1075,    1100,    1125 */
	CORE_DVFS("csi",		-1, -1, 1, KHZ,	 750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000),
	CORE_DVFS("cilab",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),
	CORE_DVFS("cilcd",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),
	CORE_DVFS("cile",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),

	CORE_DVFS("dsia",		-1, -1, 1, KHZ,	 500000,  500000,  500000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000),
	CORE_DVFS("dsib",		-1, -1, 1, KHZ,	 500000,  500000,  500000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000,  750000),

	CORE_DVFS("dsialp",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),
	CORE_DVFS("dsiblp",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),

	CORE_DVFS("sor0",		-1, -1, 1, KHZ,	 162000,  270000,  270000,  270000,  540000,  540000,  540000,  540000,  540000,  540000,  540000,  540000,  540000),
	CORE_DVFS("sor1",		-1, -1, 1, 1,	 148500000,  297000000,  297000000,  297000000,  297000000,  297000000,  594028125,  594028125,  594028125,  594028125,  594028125,  594028125,  594028125),

	CORE_DVFS("i2s0",		-1, -1, 1, KHZ,	  24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576),
	CORE_DVFS("i2s1",		-1, -1, 1, KHZ,	  24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576),
	CORE_DVFS("i2s2",		-1, -1, 1, KHZ,	  24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576),
	CORE_DVFS("i2s3",		-1, -1, 1, KHZ,	  24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576),
	CORE_DVFS("i2s4",		-1, -1, 1, KHZ,	  24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576,   24576),

	CORE_DVFS("d_audio",		-1, -1, 1, KHZ,	  49152,   98304,   98304,   98304,   98304,   98304,   98304,   98304,   98304,   98304,   98304,   98304,   98304),
	CORE_DVFS("spdif_out",		-1, -1, 1, KHZ,	  24576,   24576,   24576,   24576,   24576,   24728,   24728,   24728,   24728,   24728,   24728,   24728,   24728),
	CORE_DVFS("dmic1",		-1, -1, 1, KHZ,	  12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190),
	CORE_DVFS("dmic2",		-1, -1, 1, KHZ,	  12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190),
	CORE_DVFS("dmic3",		-1, -1, 1, KHZ,	  12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190,   12190),

	CORE_DVFS("hda",		-1, -1, 1, KHZ,	  51000,   51000,   51000,   51000,   51000,   51000,   51000,   51000,   51000,   51000,   51000,   51000,   51000),
	CORE_DVFS("hda2codec_2x",	-1, -1, 1, KHZ,	  48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000),

	/* Core voltages(mV):				    825,     850,     875,     900,     925,     950,     975,    1000,    1025,    1050,    1075,    1100,    1125 */
	CORE_DVFS("sdmmc2",		-1, -1, 1, KHZ,	 204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000),
	CORE_DVFS("sdmmc4",		-1, -1, 1, KHZ,	 200000,  266000,  266000,  266000,  266000,  266000,  266000,  266000,  266000,  266000,  266000,  266000,  266000),
	CORE_DVFS("sdmmc2_ddr",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),
	CORE_DVFS("sdmmc4_ddr",		-1, -1, 1, KHZ,	 102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),

	CORE_DVFS("sdmmc1",		-1, -1, 1, KHZ,	 136000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000),
	CORE_DVFS("sdmmc3",		-1, -1, 1, KHZ,	 136000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000),

	CORE_DVFS("sdmmc1_ddr",		-1, -1, 1, KHZ,	  96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000),
	CORE_DVFS("sdmmc3_ddr",		-1, -1, 1, KHZ,	  96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000,   96000),

	CORE_DVFS("xusb_falcon_src",	-1, -1, 1, KHZ,	      1,  336000,  336000,  336000,  336000,  336000,  336000,  336000,  336000,  336000,  336000,  336000,  336000),
	CORE_DVFS("xusb_host_src",	-1, -1, 1, KHZ,	      1,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000),
	CORE_DVFS("xusb_dev_src",	-1, -1, 1, KHZ,	      1,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000,  112000),
	CORE_DVFS("xusb_ssp_src",	-1, -1, 1, KHZ,	      1,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000),
	CORE_DVFS("xusb_fs_src",	-1, -1, 1, KHZ,	      1,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000),
	CORE_DVFS("xusb_hs_src",	-1, -1, 1, KHZ,	      1,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000,  120000),
	CORE_DVFS("usbd",		-1, -1, 1, KHZ,	 480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000),
	CORE_DVFS("usb2",		-1, -1, 1, KHZ,	 480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000),
	CORE_DVFS("usb3",		-1, -1, 1, KHZ,	 480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000,  480000),

	CORE_DVFS("sata",		-1, -1, 1, KHZ,	      1,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000,  102000),
	CORE_DVFS("sata_oob",		-1, -1, 1, KHZ,	      1,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000,  204000),
	CORE_DVFS("pciex",		-1, -1, 1, KHZ,	      1,  500000,  500000,  500000,  500000,  500000,  500000,  500000,  500000,  500000,  500000,  500000,  500000),
	CORE_DVFS("pwm",		-1, -1, 1, KHZ,	  48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000,   48000),

	CORE_DVFS("soc_therm",		-1, -1, 1, KHZ,	 136000,  136000,  136000,  136000,  136000,  136000,  136000,  136000,  136000,  136000,  136000,  136000,  136000),
	CORE_DVFS("tsensor",		-1, -1, 1, KHZ,	  19200,   19200,   19200,   19200,   19200,   19200,   19200,   19200,   19200,   19200,   19200,   19200,   19200),
};

static struct dvfs spi_dvfs_table[] = {
	CORE_DVFS("sbc1",		-1, -1, 1, KHZ,	  35000,   50000,   50000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000),
	CORE_DVFS("sbc2",		-1, -1, 1, KHZ,	  35000,   50000,   50000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000),
	CORE_DVFS("sbc3",		-1, -1, 1, KHZ,	  35000,   50000,   50000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000),
	CORE_DVFS("sbc4",		-1, -1, 1, KHZ,	  35000,   50000,   50000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000,   65000),

};

static struct dvfs spi_slave_dvfs_table[] = {
	CORE_DVFS("sbc1",		-1, -1, 1, KHZ,	  45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000),
	CORE_DVFS("sbc2",		-1, -1, 1, KHZ,	  45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000),
	CORE_DVFS("sbc3",		-1, -1, 1, KHZ,	  45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000),
	CORE_DVFS("sbc4",		-1, -1, 1, KHZ,	  45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000,   45000),

};

static struct dvfs qspi_sdr_dvfs_table[] = {
	CORE_DVFS("qspi",		-1, -1, 1, KHZ,	  81600,  116600,  116600,  116600,  116600,  116600,  116600,  116600,  116600,  116600,  116600,  116600,  116600),
};

static struct dvfs qspi_ddr_dvfs_table[] = {
	CORE_DVFS("qspi",		-1, -1, 1, KHZ,	  81600,   81600,   81600,   81600,   81600,   81600,   81600,   81600,   81600,   81600,   81600,   81600,   81600),
};

static int tegra_dvfs_disable_core_set(const char *arg,
	const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra21_dvfs_rail_vdd_core);
	else
		tegra_dvfs_rail_enable(&tegra21_dvfs_rail_vdd_core);

	return 0;
}

static int tegra_dvfs_disable_cpu_set(const char *arg,
	const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra21_dvfs_rail_vdd_cpu);
	else
		tegra_dvfs_rail_enable(&tegra21_dvfs_rail_vdd_cpu);

	return 0;
}

static int tegra_dvfs_disable_gpu_set(const char *arg,
	const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_gpu_disabled)
		tegra_dvfs_rail_disable(&tegra21_dvfs_rail_vdd_gpu);
	else
		tegra_dvfs_rail_enable(&tegra21_dvfs_rail_vdd_gpu);

	return 0;
}

static int tegra_dvfs_disable_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops tegra_dvfs_disable_core_ops = {
	.set = tegra_dvfs_disable_core_set,
	.get = tegra_dvfs_disable_get,
};

static struct kernel_param_ops tegra_dvfs_disable_cpu_ops = {
	.set = tegra_dvfs_disable_cpu_set,
	.get = tegra_dvfs_disable_get,
};

static struct kernel_param_ops tegra_dvfs_disable_gpu_ops = {
	.set = tegra_dvfs_disable_gpu_set,
	.get = tegra_dvfs_disable_get,
};

module_param_cb(disable_core, &tegra_dvfs_disable_core_ops,
	&tegra_dvfs_core_disabled, 0644);
module_param_cb(disable_cpu, &tegra_dvfs_disable_cpu_ops,
	&tegra_dvfs_cpu_disabled, 0644);
module_param_cb(disable_gpu, &tegra_dvfs_disable_gpu_ops,
	&tegra_dvfs_gpu_disabled, 0644);

static bool __init match_dvfs_one(const char *name,
	int dvfs_speedo_id, int dvfs_process_id,
	int speedo_id, int process_id)
{
	if ((dvfs_process_id != -1 && dvfs_process_id != process_id) ||
		(dvfs_speedo_id != -1 && dvfs_speedo_id != speedo_id)) {
		pr_debug("tegra21_dvfs: rejected %s speedo %d, process %d\n",
			 name, dvfs_speedo_id, dvfs_process_id);
		return false;
	}
	return true;
}

/* cvb_mv = ((c2 * speedo / s_scale + c1) * speedo / s_scale + c0) / v_scale */
static inline int get_cvb_voltage(int speedo, int s_scale,
				  struct cvb_dvfs_parameters *cvb)
{
	/* apply only speedo scale: output mv = cvb_mv * v_scale */
	int mv;
	mv = DIV_ROUND_CLOSEST(cvb->c2 * speedo, s_scale);
	mv = DIV_ROUND_CLOSEST((mv + cvb->c1) * speedo, s_scale) + cvb->c0;
	return mv;
}

/* cvb_t_mv =
   ((c3 * speedo / s_scale + c4 + c5 * T / t_scale) * T / t_scale) / v_scale */
static inline int get_cvb_t_voltage(int speedo, int s_scale, int t, int t_scale,
				    struct cvb_dvfs_parameters *cvb)
{
	/* apply speedo & temperature scales: output mv = cvb_t_mv * v_scale */
	int mv;
	mv = DIV_ROUND_CLOSEST(cvb->c3 * speedo, s_scale) + cvb->c4 +
		DIV_ROUND_CLOSEST(cvb->c5 * t, t_scale);
	mv = DIV_ROUND_CLOSEST(mv * t, t_scale);
	return mv;
}

static int round_cvb_voltage(int mv, int v_scale, struct rail_alignment *align)
{
	/* combined: apply voltage scale and round to cvb alignment step */
	int uv;
	int step = (align->step_uv ? : 1000) * v_scale;
	int offset = align->offset_uv * v_scale;

	uv = max(mv * 1000, offset) - offset;
	uv = DIV_ROUND_UP(uv, step) * align->step_uv + align->offset_uv;
	return uv / 1000;
}

static int round_voltage(int mv, struct rail_alignment *align, bool up)
{
	if (align->step_uv) {
		int uv = max(mv * 1000, align->offset_uv) - align->offset_uv;
		uv = (uv + (up ? align->step_uv - 1 : 0)) / align->step_uv;
		return (uv * align->step_uv + align->offset_uv) / 1000;
	}
	return mv;
}

/*
 * Setup fast CPU DVFS tables in PLL and DFLL modes from CVB data, determine
 * nominal voltage for CPU rail, and CPU maximum frequency. Note that entire
 * frequency range is guaranteed only when DFLL is used as CPU clock source.
 * Reaching maximum frequency on PLL may not be possible within nominal voltage
 * range (DVFS core would fail frequency request in this case, so that voltage
 * limit is not violated). Error when CPU DVFS table can not be constructed must
 * never happen.
 */
static int __init set_cpu_dvfs_data(unsigned long max_freq,
	struct cpu_cvb_dvfs *d, struct dvfs *cpu_dvfs, int *max_freq_index)
{
	int j, mv, min_mv, dfll_mv, min_dfll_mv;
	unsigned long fmax_at_vmin = 0;
	unsigned long fmax_pll_mode = 0;
	unsigned long fmin_use_dfll = 0;
	int speedo = tegra_cpu_speedo_value();

	struct cvb_dvfs_table *table = NULL;
	struct dvfs_rail *rail = &tegra21_dvfs_rail_vdd_cpu;
	struct rail_alignment *align = &rail->alignment;

	min_dfll_mv = d->dfll_tune_data.min_millivolts;
	if (min_dfll_mv < rail->min_millivolts) {
		pr_debug("tegra21_dvfs: dfll min %dmV below rail min %dmV\n",
		     min_dfll_mv, rail->min_millivolts);
		min_dfll_mv = rail->min_millivolts;
	}
	min_dfll_mv =  round_voltage(min_dfll_mv, align, true);

	min_mv = d->pll_tune_data.min_millivolts;
	if (min_mv < rail->min_millivolts) {
		pr_debug("tegra21_dvfs: pll min %dmV below rail min %dmV\n",
		     min_mv, rail->min_millivolts);
		min_mv = rail->min_millivolts;
	}
	min_mv =  round_voltage(min_mv, align, true);

	d->max_mv = round_voltage(d->max_mv, align, false);
	BUG_ON(d->max_mv > rail->max_millivolts);

	/*
	 * Use CVB table to fill in CPU dvfs frequencies and voltages. Each
	 * CVB entry specifies CPU frequency and CVB coefficients to calculate
	 * the respective voltage when either DFLL or PLL is used as CPU clock
	 * source.
	 *
	 * Different minimum voltage limits are applied to DFLL and PLL sources.
	 * Same maximum voltage limit is used for both sources, but differently:
	 * directly limit voltage for DFLL, and limit maximum frequency for PLL.
	 */
	for (j = 0; j < MAX_DVFS_FREQS; j++) {
		table = &d->cvb_table[j];
		if (!table->freq || (table->freq > max_freq))
			break;

		dfll_mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_dfll_param);
		dfll_mv = round_cvb_voltage(dfll_mv, d->voltage_scale, align);
		dfll_mv = max(dfll_mv, min_dfll_mv);

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);
#ifdef BRINGUP_CVB_V_MARGIN
		mv = mv * (100 + BRINGUP_CVB_V_MARGIN +
			   BRINGUP_CVB_V_MARGIN_EX) / 100;
#endif
		mv = round_cvb_voltage(mv, d->voltage_scale, align);
		mv = max(mv, min_mv);

		/*
		 * Check maximum frequency at minimum voltage for dfll source;
		 * round down unless all table entries are above Vmin, then use
		 * the 1st entry as is.
		 */
		if (dfll_mv > min_dfll_mv) {
			if (!j)
				fmax_at_vmin = table->freq;
			if (!fmax_at_vmin)
				fmax_at_vmin = cpu_dvfs->freqs[j - 1];
		}

		/* Clip maximum frequency at maximum voltage for pll source */
		if (mv > d->max_mv) {
			if (!j)
				break;	/* 1st entry already above Vmax */
			if (!fmax_pll_mode)
				fmax_pll_mode = cpu_dvfs->freqs[j - 1];
		}

		/* Minimum rate with pll source voltage above dfll Vmin */
		if ((mv >= min_dfll_mv) && (!fmin_use_dfll))
			fmin_use_dfll = table->freq;

		/* fill in dvfs tables */
		cpu_dvfs->freqs[j] = table->freq;
		cpu_dfll_millivolts[j] = min(dfll_mv, d->max_mv);
		cpu_millivolts[j] = mv;
	}

	/* Table must not be empty, must have at least one entry above Vmin */
	if (!j || !fmax_at_vmin) {
		pr_err("tegra21_dvfs: invalid cpu dvfs table\n");
		return -ENOENT;
	}

	/* In the dfll operating range dfll voltage at any rate should be
	   better (below) than pll voltage */
	if (!fmin_use_dfll || (fmin_use_dfll > fmax_at_vmin)) {
		WARN(1, "tegra21_dvfs: pll voltage is below dfll in the dfll"
			" operating range\n");
		fmin_use_dfll = fmax_at_vmin;
	}

	/* dvfs tables are successfully populated - fill in the rest */
	cpu_dvfs->speedo_id = d->speedo_id;
	cpu_dvfs->boost_table = d->speedo_id == 2;
	cpu_dvfs->process_id = d->process_id;
	cpu_dvfs->freqs_mult = d->freqs_mult;
	cpu_dvfs->dvfs_rail->nominal_millivolts = min(d->max_mv,
		max(cpu_millivolts[j - 1], cpu_dfll_millivolts[j - 1]));
	*max_freq_index = j - 1;

	cpu_dvfs->dfll_data = d->dfll_tune_data;
	cpu_dvfs->dfll_data.max_rate_boost = fmax_pll_mode ?
		(cpu_dvfs->freqs[j - 1] - fmax_pll_mode) * d->freqs_mult : 0;
	cpu_dvfs->dfll_data.out_rate_min = fmax_at_vmin * d->freqs_mult;
	cpu_dvfs->dfll_data.use_dfll_rate_min = fmin_use_dfll * d->freqs_mult;
	cpu_dvfs->dfll_data.min_millivolts = min_dfll_mv;

	return 0;
}

static void __init init_cpu_dvfs_table(int *cpu_max_freq_index)
{
	int i, ret;
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int cpu_process_id = tegra_cpu_process_id();

	BUG_ON(cpu_speedo_id >= ARRAY_SIZE(cpu_max_freq));
	for (ret = 0, i = 0; i <  ARRAY_SIZE(cpu_cvb_dvfs_table); i++) {
		struct cpu_cvb_dvfs *d = &cpu_cvb_dvfs_table[i];
		unsigned long max_freq = cpu_max_freq[cpu_speedo_id];
		if (match_dvfs_one("cpu cvb", d->speedo_id, d->process_id,
				   cpu_speedo_id, cpu_process_id)) {
			ret = set_cpu_dvfs_data(max_freq,
				d, &cpu_dvfs, cpu_max_freq_index);
			break;
		}
	}
	BUG_ON((i == ARRAY_SIZE(cpu_cvb_dvfs_table)) || ret);
}

/*
 * Setup slow CPU (a.k.a LP CPU) DVFS table from CVB data. Only PLL is used as
 * a clock source for slow CPU. Its maximum frequency must be reached within
 * nominal voltage -- CVB frequency list is cut off at rate that exceeds either
 * sku-based maximum limit or requires voltage above nominal. Error when DVFS
 * table can not be constructed must never happen.
 *
 * Final CPU rail nominal voltage is set as maximum of fast and slow CPUs
 * nominal voltages.
 */
static int __init set_cpu_lp_dvfs_data(unsigned long max_freq,
	struct cpu_cvb_dvfs *d, struct dvfs *cpu_lp_dvfs, int *max_freq_index)
{
	int j, mv, min_mv;
	int speedo = tegra_cpu_speedo_value(); /* FIXME cpu_lp_speedo */

	struct cvb_dvfs_table *table = NULL;
	struct dvfs_rail *rail = &tegra21_dvfs_rail_vdd_cpu;
	struct rail_alignment *align = &rail->alignment;

	min_mv = d->pll_tune_data.min_millivolts;
	if (min_mv < rail->min_millivolts) {
		pr_debug("tegra21_dvfs: scpu min %dmV below rail min %dmV\n",
		     min_mv, rail->min_millivolts);
		min_mv = rail->min_millivolts;
	}
	min_mv =  round_voltage(min_mv, align, true);

	d->max_mv = round_voltage(d->max_mv, align, false);
	BUG_ON(d->max_mv > rail->max_millivolts);
	rail->nominal_millivolts = max(rail->nominal_millivolts, d->max_mv);

	/*
	 * Use CVB table to fill in CPU dvfs frequencies and voltages. Each
	 * CVB entry specifies CPU frequency and CVB coefficients to calculate
	 * the respective voltage. Only PLL is used as CPU LP clock source, and
	 * the respective minimum limit is applied to each table entry. Table
	 * construction is aborted if calculated voltage is above maximum limit.
	 */
	for (j = 0; j < MAX_DVFS_FREQS; j++) {
		table = &d->cvb_table[j];
		if (!table->freq || (table->freq > max_freq))
			break;

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);
		mv = round_cvb_voltage(mv, d->voltage_scale, align);
		mv = max(mv, min_mv);

		if (mv > d->max_mv) {
			pr_warn("tegra21_dvfs: %dmV for %s rate %lu above limit %dmV\n",
			     mv, cpu_lp_dvfs->clk_name, table->freq, d->max_mv);
			break;
		}

		/* fill in dvfs tables */
		cpu_lp_dvfs->freqs[j] = table->freq;
		cpu_lp_millivolts[j] = mv;
	}

	/* Table must not be empty */
	if (!j) {
		pr_err("tegra21_dvfs: invalid cpu lp dvfs table\n");
		return -ENOENT;
	}

	/* dvfs tables are successfully populated - fill in the rest */
	cpu_lp_dvfs->speedo_id = d->speedo_id;
	cpu_lp_dvfs->process_id = d->process_id;
	cpu_lp_dvfs->freqs_mult = d->freqs_mult;
	*max_freq_index = j - 1;

	return 0;
}

static void __init init_cpu_lp_dvfs_table(int *cpu_lp_max_freq_index)
{
	int i, ret;
	int cpu_lp_speedo_id = tegra_cpu_speedo_id(); /* FIXME cpu_lp_ */
	int cpu_lp_process_id = tegra_cpu_process_id(); /* FIXME cpu_lp_ */

	BUG_ON(cpu_lp_speedo_id >= ARRAY_SIZE(cpu_lp_max_freq));
	for (ret = 0, i = 0; i <  ARRAY_SIZE(cpu_lp_cvb_dvfs_table); i++) {
		struct cpu_cvb_dvfs *d = &cpu_lp_cvb_dvfs_table[i];
		unsigned long max_freq = cpu_lp_max_freq[cpu_lp_speedo_id];
		if (match_dvfs_one("cpu lp cvb", d->speedo_id, d->process_id,
				   cpu_lp_speedo_id, cpu_lp_process_id)) {
			ret = set_cpu_lp_dvfs_data(max_freq,
				d, &cpu_lp_dvfs, cpu_lp_max_freq_index);
			break;
		}
	}
	BUG_ON((i == ARRAY_SIZE(cpu_lp_cvb_dvfs_table)) || ret);
}

static int __init init_cpu_rail_thermal_profile(struct dvfs *cpu_dvfs)
{
	struct dvfs_rail *rail = &tegra21_dvfs_rail_vdd_cpu;

	/*
	 * Failure to get/configure trips may not be fatal for boot - let it
	 * boot, even with partial configuration with appropriate WARNING, and
	 * invalidate cdev. It must not happen with production DT, of course.
	 */
	if (rail->vmin_cdev) {
		if (tegra_dvfs_rail_of_init_vmin_thermal_profile(
			vdd_cpu_vmin_trips_table, vdd_cpu_therm_floors_table,
			rail, &cpu_dvfs->dfll_data))
			rail->vmin_cdev = NULL;
	}

	if (rail->vmax_cdev) {
		if (tegra_dvfs_rail_of_init_vmax_thermal_profile(
			vdd_cpu_vmax_trips_table, vdd_cpu_therm_caps_table,
			rail, &cpu_dvfs->dfll_data))
			rail->vmax_cdev = NULL;
	}

	return 0;
}

/*
 * Setup gpu dvfs tables from cvb data, determine nominal voltage for gpu rail,
 * and gpu maximum frequency. Error when gpu dvfs table can not be constructed
 * must never happen.
 */
static unsigned long __init find_gpu_fmax_at_vmin(
	struct dvfs *gpu_dvfs, int thermal_ranges, int freqs_num)
{
	int i, j;
	unsigned long fmax = ULONG_MAX;

	/*
	 * For voltage scaling row in each temperature range, as well as peak
	 * voltage row find maximum frequency at lowest voltage, and return
	 * minimax. On Tegra21 all GPU DVFS thermal dependencies are integrated
	 * into thermal DVFS table (i.e., there is no separate thermal floors
	 * applied in the rail level). Hence, returned frequency specifies max
	 * frequency safe at minimum voltage across all temperature ranges.
	 */
	for (j = 0; j < thermal_ranges; j++) {
		for (i = 1; i < freqs_num; i++) {
			if (gpu_millivolts[j][i] > gpu_millivolts[j][0])
				break;
		}
		fmax = min(fmax, gpu_dvfs->freqs[i - 1]);
	}

	for (i = 1; i < freqs_num; i++) {
		if (gpu_peak_millivolts[i] > gpu_peak_millivolts[0])
			break;
	}
	fmax = min(fmax, gpu_dvfs->freqs[i - 1]);

	return fmax;
}

/*
 * Init thermal trips, find number of thermal ranges; note that the first
 * trip-point is used for voltage calculations within the lowest range, but
 * should not be actually set. Hence, at least 2 trip-points must be specified.
 *
 * Failure to get/configure trips may not be fatal for boot - let it try,
 * anyway, with appropriate WARNING. It must not happen with production DT, of
 * course.
 */
static int __init init_gpu_rail_thermal_profile(struct dvfs_rail *rail,
						struct gpu_cvb_dvfs *d)
{
	int thermal_ranges = 1;	/* No thermal depndencies */

	if (!rail->vts_cdev)
		return 1;

	thermal_ranges = of_tegra_dvfs_rail_get_cdev_trips(
		rail->vts_cdev, d->vts_trips_table, d->therm_floors_table,
		&rail->alignment, true);

	if (thermal_ranges < 0) {
		WARN(1, "tegra21_dvfs: %s: failed to get trips from DT\n",
		     rail->reg_id);
		return 1;
	}

	if (thermal_ranges < 2) {
		WARN(1, "tegra21_dvfs: %s: only %d trip (must be at least 2)\n",
		     rail->reg_id, thermal_ranges);
		return 1;
	}

	rail->vts_cdev->trip_temperatures_num = thermal_ranges - 1;
	rail->vts_cdev->trip_temperatures = d->vts_trips_table;
	return thermal_ranges;
}

static int __init set_gpu_dvfs_data(unsigned long max_freq,
	struct gpu_cvb_dvfs *d, struct dvfs *gpu_dvfs, int *max_freq_index)
{
	int i, j, thermal_ranges, mv;
	struct cvb_dvfs_table *table = NULL;
	int speedo = tegra_gpu_speedo_value();
	struct dvfs_rail *rail = &tegra21_dvfs_rail_vdd_gpu;
	struct rail_alignment *align = &rail->alignment;

	d->max_mv = round_voltage(d->max_mv, align, false);

	/*
	 * Get scaling thermal ranges; 1 range implies no thermal dependency.
	 * Invalidate scaling cooling device in the latter case.
	 */
	thermal_ranges = init_gpu_rail_thermal_profile(rail, d);
	if (thermal_ranges == 1)
		rail->vts_cdev = NULL;

	/*
	 * Use CVB table to calculate Vmin for each temperature range
	 */
	mv = get_cvb_voltage(
		speedo, d->speedo_scale, &d->cvb_vmin.cvb_pll_param);
	for (j = 0; j < thermal_ranges; j++) {
		int mvj = mv;
		int t = thermal_ranges == 1 ? 0 :
			rail->vts_cdev->trip_temperatures[j];

		/* add Vmin thermal offset for this trip-point */
		mvj += get_cvb_t_voltage(speedo, d->speedo_scale,
			t, d->thermal_scale, &d->cvb_vmin.cvb_pll_param);
		mvj = round_cvb_voltage(mvj, d->voltage_scale, align);
		if (mvj < rail->min_millivolts) {
			pr_debug("tegra21_dvfs: gpu min %dmV below rail min %dmV\n",
			     mvj, rail->min_millivolts);
			mvj = rail->min_millivolts;
		}

		gpu_vmin[j] = mvj;
	}

	/*
	 * Use CVB table to fill in gpu dvfs frequencies and voltages. Each
	 * CVB entry specifies gpu frequency and CVB coefficients to calculate
	 * the respective voltage.
	 */
	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		table = &d->cvb_table[i];
		if (!table->freq || (table->freq > max_freq))
			break;

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);
		for (j = 0; j < thermal_ranges; j++) {
			int mvj = mv;
			int t = thermal_ranges == 1 ? 0 :
				rail->vts_cdev->trip_temperatures[j];

			/* get thermal offset for this trip-point */
			mvj += get_cvb_t_voltage(speedo, d->speedo_scale,
				t, d->thermal_scale, &table->cvb_pll_param);
			mvj = round_cvb_voltage(mvj, d->voltage_scale, align);

			/* clip to minimum, abort if above maximum */
			mvj = max(mvj, gpu_vmin[j]);
			if (mvj > d->max_mv)
				break;

			/*
			 * Apply fixed thermal floor, and  update voltage for
			 * adjacent ranges bounded by this trip-point (cvb &
			 * dvfs are transpose matrices)
			 */
			gpu_millivolts[j][i] = max(mvj,
						   d->therm_floors_table[j]);
			if (j && (gpu_millivolts[j-1][i] < mvj))
				gpu_millivolts[j-1][i] = mvj;
		}
		/* Make sure all voltages for this frequency are below max */
		if (j < thermal_ranges)
			break;

		/* fill in gpu dvfs tables */
		gpu_dvfs->freqs[i] = table->freq;
	}

	/*
	 * Table must not be empty, must have at least one entry in range, and
	 * must specify monotonically increasing voltage on frequency dependency
	 * in each temperature range.
	 */
	if (!i || tegra_dvfs_init_thermal_dvfs_voltages(&gpu_millivolts[0][0],
		gpu_peak_millivolts, i, thermal_ranges, gpu_dvfs)) {
		pr_err("tegra21_dvfs: invalid gpu dvfs table\n");
		return -ENOENT;
	}

	/* Shift out the 1st trip-point */
	for (j = 1; j < thermal_ranges; j++)
		rail->vts_cdev->trip_temperatures[j - 1] =
		rail->vts_cdev->trip_temperatures[j];

	/* dvfs tables are successfully populated - fill in the gpu dvfs */
	gpu_dvfs->speedo_id = d->speedo_id;
	gpu_dvfs->process_id = d->process_id;
	gpu_dvfs->freqs_mult = d->freqs_mult;
	gpu_dvfs->dvfs_rail->nominal_millivolts = d->max_mv;

	*max_freq_index = i - 1;

	gpu_dvfs->fmax_at_vmin_safe_t = d->freqs_mult *
		find_gpu_fmax_at_vmin(gpu_dvfs, thermal_ranges, i);

#ifdef CONFIG_TEGRA_USE_NA_GPCPLL
	/*
	 * Set NA DVFS flag, if GPCPLL NA mode is enabled. This is necessary to
	 * make sure that GPCPLL configuration is updated by tegra core DVFS
	 * when thermal DVFS cooling device state is changed. Since tegra core
	 * DVFS does not support NA operations for Vmin cooling device, GPU Vmin
	 * thermal floors have been integrated with thermal DVFS, and no Vmin
	 * cooling device is installed.
	 */
	if (tegra_fuse_can_use_na_gpcpll())
		gpu_dvfs->na_dvfs = 1;
#endif
	return 0;
}

static void __init init_gpu_dvfs_table(int *gpu_max_freq_index)
{
	int i, ret;
	int gpu_speedo_id = tegra_gpu_speedo_id();
	int gpu_process_id = tegra_gpu_process_id();

	BUG_ON(gpu_speedo_id >= ARRAY_SIZE(gpu_max_freq));
	for (ret = 0, i = 0; i < ARRAY_SIZE(gpu_cvb_dvfs_table); i++) {
		struct gpu_cvb_dvfs *d = &gpu_cvb_dvfs_table[i];
		unsigned long max_freq = gpu_max_freq[gpu_speedo_id];
		if (match_dvfs_one("gpu cvb", d->speedo_id, d->process_id,
				   gpu_speedo_id, gpu_process_id)) {
			ret = set_gpu_dvfs_data(max_freq,
				d, &gpu_dvfs, gpu_max_freq_index);
			break;
		}
	}
	BUG_ON((i == ARRAY_SIZE(gpu_cvb_dvfs_table)) || ret);
}


/*
 * SPI DVFS tables are different in master and in slave mode. Use master tables
 * by default. Check if slave mode is specified for enabled SPI devices in DT,
 * and overwrite master table for the respective SPI controller.
 */

static __initdata struct {
	u64 address;
	struct dvfs *d;
} spi_map[] = {
	{ 0x7000d400, &spi_dvfs_table[0] },
	{ 0x7000d600, &spi_dvfs_table[1] },
	{ 0x7000d800, &spi_dvfs_table[2] },
	{ 0x7000da00, &spi_dvfs_table[3] },
};

static int __init of_update_spi_slave_dvfs(struct device_node *dn)
{
	int i;
	u64 addr = 0;
	const __be32 *reg;

	if (!of_device_is_available(dn))
		return 0;

	reg = of_get_property(dn, "reg", NULL);
	if (reg && of_can_translate_address(dn))
		addr = of_translate_address(dn, reg);

	for (i = 0; i < ARRAY_SIZE(spi_map); i++) {
		if (spi_map[i].address == addr) {
			spi_map[i].d = &spi_slave_dvfs_table[i];
			break;
		}
	}
	return 0;
}

static __initdata struct of_device_id tegra21_dvfs_spi_slave_of_match[] = {
	{ .compatible = "nvidia,tegra210-spi-slave",
	  .data = of_update_spi_slave_dvfs, },
	{ },
};

static void __init init_spi_dvfs(int soc_speedo_id, int core_process_id,
				 int core_nominal_mv_index)
{
	int i;

	of_tegra_dvfs_init(tegra21_dvfs_spi_slave_of_match);

	for (i = 0; i <  ARRAY_SIZE(spi_map); i++) {
		struct dvfs *d = spi_map[i].d;
		if (!match_dvfs_one(d->clk_name, d->speedo_id,
			d->process_id, soc_speedo_id, core_process_id))
			continue;
		tegra_init_dvfs_one(d, core_nominal_mv_index);
	}
}

/*
 * QSPI DVFS tables are different in SDR and DDR modes. Use SDR tables by
 * default. Check if DDR mode is specified for enabled QSPI device in DT,
 * and overwrite DVFS table, respectively.
 */

static __initdata struct dvfs *qspi_dvfs = &qspi_sdr_dvfs_table[0];

static int __init of_update_qspi_dvfs(struct device_node *dn)
{
	if (of_device_is_available(dn)) {
		if (of_get_property(dn, "nvidia,x4-is-ddr", NULL))
			qspi_dvfs = &qspi_ddr_dvfs_table[0];
	}
	return 0;
}

static __initdata struct of_device_id tegra21_dvfs_qspi_of_match[] = {
	{ .compatible = "nvidia,tegra210-qspi", .data = of_update_qspi_dvfs, },
	{ },
};

static void __init init_qspi_dvfs(int soc_speedo_id, int core_process_id,
				  int core_nominal_mv_index)
{
	int i;
	struct device_node *dn;

	of_tegra_dvfs_init(tegra21_dvfs_qspi_of_match);

	if (match_dvfs_one(qspi_dvfs->clk_name, qspi_dvfs->speedo_id,
		qspi_dvfs->process_id, soc_speedo_id, core_process_id))
		tegra_init_dvfs_one(qspi_dvfs, core_nominal_mv_index);
}

/*
 * Clip sku-based core nominal voltage to core DVFS voltage ladder
 */
static int __init get_core_nominal_mv_index(int speedo_id)
{
	int i;
	int mv = tegra_core_speedo_mv();
	int core_edp_voltage = get_core_edp();

	/*
	 * Start with nominal level for the chips with this speedo_id. Then,
	 * make sure core nominal voltage is below edp limit for the board
	 * (if edp limit is set).
	 */
	if (!core_edp_voltage)
		core_edp_voltage = 1125;	/* default 1.125V EDP limit */

	mv = min(mv, core_edp_voltage);
#ifndef CONFIG_TEGRA_CORE_DVFS
	mv = min(mv, 1000);		/* Vmax if scaling is disabled */
#endif
	/* Round nominal level down to the nearest core scaling step */
	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		if ((core_millivolts[i] == 0) || (mv < core_millivolts[i]))
			break;
	}

	if (i == 0) {
		pr_err("tegra21_dvfs: unable to adjust core dvfs table to"
		       " nominal voltage %d\n", mv);
		return -ENOSYS;
	}
	return i - 1;
}

static int __init init_core_rail_thermal_profile(void)
{
	struct dvfs_rail *rail = &tegra21_dvfs_rail_vdd_core;

	/*
	 * Failure to get/configure trips may not be fatal for boot - let it
	 * boot, even with partial configuration with appropriate WARNING, and
	 * invalidate cdev. It must not happen with production DT, of course.
	 */
	if (rail->vmin_cdev) {
		if (tegra_dvfs_rail_of_init_vmin_thermal_profile(
			vdd_core_vmin_trips_table, vdd_core_therm_floors_table,
			rail, NULL))
			rail->vmin_cdev = NULL;
	}

	return 0;
}

static int __init of_rails_init(struct device_node *dn)
{
	int i;

	if (!of_device_is_available(dn))
		return 0;

	for (i = 0; i < ARRAY_SIZE(tegra21_dvfs_rails); i++) {
		struct dvfs_rail *rail = tegra21_dvfs_rails[i];
		if (!of_tegra_dvfs_rail_node_parse(dn, rail)) {
			rail->stats.bin_uV = rail->alignment.step_uv;
			return 0;
		}
	}
	return -ENOENT;
}

static __initdata struct of_device_id tegra21_dvfs_rail_of_match[] = {
	{ .compatible = "nvidia,tegra210-dvfs-rail", .data = of_rails_init, },
	{ },
};

void __init tegra21x_init_dvfs(void)
{
	int soc_speedo_id = tegra_soc_speedo_id();
	int core_process_id = tegra_core_process_id();

	int i, ret;
	int core_nominal_mv_index;
	int gpu_max_freq_index = 0;
	int cpu_max_freq_index = 0;
	int cpu_lp_max_freq_index = 0;

#ifndef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_core_disabled = true;
#endif
#ifndef CONFIG_TEGRA_CPU_DVFS
	tegra_dvfs_cpu_disabled = true;
#endif
#ifndef CONFIG_TEGRA_GPU_DVFS
	tegra_dvfs_gpu_disabled = true;
#endif

	of_tegra_dvfs_init(tegra21_dvfs_rail_of_match);

	/*
	 * Find nominal voltages for core (1st) and cpu rails before rail
	 * init. Nominal voltage index in core scaling ladder can also be
	 * used to determine max dvfs frequencies for all core clocks. In
	 * case of error disable core scaling and set index to 0, so that
	 * core clocks would not exceed rates allowed at minimum voltage.
	 */
	core_nominal_mv_index = get_core_nominal_mv_index(soc_speedo_id);
	if (core_nominal_mv_index < 0) {
		tegra21_dvfs_rail_vdd_core.disabled = true;
		tegra_dvfs_core_disabled = true;
		core_nominal_mv_index = 0;
	}
	tegra21_dvfs_rail_vdd_core.nominal_millivolts =
		core_millivolts[core_nominal_mv_index];

	/*
	 * Construct fast and slow CPU DVFS tables from CVB data; find maximum
	 * frequency, minimum  and nominal voltage for each CPU cluster, and
	 * combined rail limits (fast CPU should be initialized 1st).
	 */
	init_cpu_dvfs_table(&cpu_max_freq_index);
	init_cpu_lp_dvfs_table(&cpu_lp_max_freq_index);

	/* Init cpu thermal profile */
	init_cpu_rail_thermal_profile(&cpu_dvfs);

	/*
	 * Construct GPU DVFS table from CVB data; find GPU maximum frequency,
	 * and nominal voltage.
	 */
	init_gpu_dvfs_table(&gpu_max_freq_index);

	/* Init core thermal profile */
	init_core_rail_thermal_profile();

	/* Init rail structures and dependencies */
	tegra_dvfs_init_rails(tegra21_dvfs_rails,
		ARRAY_SIZE(tegra21_dvfs_rails));

	/* Search core dvfs table for speedo/process matching entries and
	   initialize dvfs-ed clocks */
	if (!tegra_platform_is_linsim()) {
		for (i = 0; i <  ARRAY_SIZE(core_dvfs_table); i++) {
			struct dvfs *d = &core_dvfs_table[i];
			if (!match_dvfs_one(d->clk_name, d->speedo_id,
				d->process_id, soc_speedo_id, core_process_id))
				continue;
			tegra_init_dvfs_one(d, core_nominal_mv_index);
		}
		init_spi_dvfs(soc_speedo_id, core_process_id,
			      core_nominal_mv_index);
		init_qspi_dvfs(soc_speedo_id, core_process_id,
			       core_nominal_mv_index);
	}

	/* Initialize matching gpu dvfs entry already found when nominal
	   voltage was determined */
	tegra_init_dvfs_one(&gpu_dvfs, gpu_max_freq_index);

	/* Initialize matching cpu dvfs entry already found when nominal
	   voltage was determined */
	tegra_init_dvfs_one(&cpu_dvfs, cpu_max_freq_index);
	tegra_init_dvfs_one(&cpu_lp_dvfs, cpu_lp_max_freq_index);

	/* Finally disable dvfs on rails if necessary */
	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra21_dvfs_rail_vdd_core);
	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra21_dvfs_rail_vdd_cpu);
	if (tegra_dvfs_gpu_disabled)
		tegra_dvfs_rail_disable(&tegra21_dvfs_rail_vdd_gpu);

	for (i = 0; i < ARRAY_SIZE(tegra21_dvfs_rails); i++) {
		struct dvfs_rail *rail = tegra21_dvfs_rails[i];
		pr_info("tegra dvfs: %s: nominal %dmV, offset %duV, step %duV, scaling %s\n",
			rail->reg_id, rail->nominal_millivolts,
			rail->alignment.offset_uv, rail->alignment.step_uv,
			rail->disabled ? "disabled" : "enabled");
	}
}

int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{
	return 0;
}

int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{
	return 0;
}

/* Core voltage and bus cap object and tables */
static struct kobject *cap_kobj;
static struct kobject *gpu_kobj;
static struct kobject *emc_kobj;

static struct core_dvfs_cap_table tegra21_core_cap_table[] = {
	{ .cap_name = "cap.vcore.c2bus" },
	{ .cap_name = "cap.vcore.c3bus" },
	{ .cap_name = "cap.vcore.sclk" },
	{ .cap_name = "cap.vcore.emc" },
	{ .cap_name = "cap.vcore.host1x" },
	{ .cap_name = "cap.vcore.mselect" },
	{ .cap_name = "cap.vcore.ape" },
};

static struct core_bus_limit_table tegra21_gpu_cap_syfs = {
	.limit_clk_name = "cap.profile.gbus",
	.refcnt_attr = {.attr = {.name = "gpu_cap_state", .mode = 0644} },
	.level_attr  = {.attr = {.name = "gpu_cap_rate", .mode = 0644} },
	.pm_qos_class = PM_QOS_GPU_FREQ_MAX,
};

static struct core_bus_limit_table tegra21_gpu_floor_sysfs = {
	.limit_clk_name = "floor.profile.gbus",
	.refcnt_attr = {.attr = {.name = "gpu_floor_state", .mode = 0644} },
	.level_attr  = {.attr = {.name = "gpu_floor_rate", .mode = 0644} },
	.pm_qos_class = PM_QOS_GPU_FREQ_MIN,
};

static struct core_bus_rates_table tegra21_gpu_rates_sysfs = {
	.bus_clk_name = "gbus",
	.rate_attr = {.attr = {.name = "gpu_rate", .mode = 0444} },
	.available_rates_attr = {
		.attr = {.name = "gpu_available_rates", .mode = 0444} },
};

static struct core_bus_limit_table tegra21_emc_floor_sysfs = {
	.limit_clk_name = "floor.profile.emc",
	.refcnt_attr = {.attr = {.name = "emc_floor_state", .mode = 0644} },
	.level_attr  = {.attr = {.name = "emc_floor_rate", .mode = 0644} },
	.pm_qos_class = PM_QOS_EMC_FREQ_MIN,
};

static struct core_bus_rates_table tegra21_emc_rates_sysfs = {
	.bus_clk_name = "emc",
	.rate_attr = {.attr = {.name = "emc_rate", .mode = 0444} },
	.available_rates_attr = {
		.attr = {.name = "emc_available_rates", .mode = 0444} },
};

static int __init tegra21_dvfs_init_core_cap(void)
{
	int ret = 0;

	cap_kobj = kobject_create_and_add("tegra_cap", kernel_kobj);
	if (!cap_kobj) {
		pr_err("tegra21_dvfs: failed to create sysfs cap object\n");
		return 0;
	}

	if (!tegra_platform_is_qt())
		ret = tegra_init_core_cap(
			tegra21_core_cap_table,
			ARRAY_SIZE(tegra21_core_cap_table),
			core_millivolts, ARRAY_SIZE(core_millivolts), cap_kobj);

	if (ret) {
		pr_err("tegra21_dvfs: failed to init core cap interface (%d)\n",
		       ret);
		kobject_del(cap_kobj);
		return 0;
	}
	tegra_core_cap_debug_init();
	pr_info("tegra dvfs: tegra sysfs cap interface is initialized\n");

	gpu_kobj = kobject_create_and_add("tegra_gpu", kernel_kobj);
	if (!gpu_kobj) {
		pr_err("tegra21_dvfs: failed to create sysfs gpu object\n");
		return 0;
	}

	ret = tegra_init_shared_bus_cap(&tegra21_gpu_cap_syfs,
					1, gpu_kobj);
	if (ret) {
		pr_err("tegra21_dvfs: failed to init gpu cap interface (%d)\n",
		       ret);
		kobject_del(gpu_kobj);
		return 0;
	}

	ret = tegra_init_shared_bus_floor(&tegra21_gpu_floor_sysfs,
					  1, gpu_kobj);
	if (ret) {
		pr_err("tegra21_dvfs: failed to init gpu floor interface (%d)\n",
		       ret);
		kobject_del(gpu_kobj);
		return 0;
	}

	ret = tegra_init_sysfs_shared_bus_rate(&tegra21_gpu_rates_sysfs,
					       1, gpu_kobj);
	if (ret) {
		pr_err("tegra21_dvfs: failed to init gpu rates interface (%d)\n",
		       ret);
		kobject_del(gpu_kobj);
		return 0;
	}

	emc_kobj = kobject_create_and_add("tegra_emc", kernel_kobj);
	if (!emc_kobj) {
		pr_err("tegra21_dvfs: failed to create sysfs emc object\n");
		return 0;
	}

	ret = tegra_init_sysfs_shared_bus_rate(&tegra21_emc_rates_sysfs,
					       1, emc_kobj);
	if (ret) {
		pr_err("tegra21_dvfs: failed to init emc rates interface (%d)\n",
		       ret);
		kobject_del(emc_kobj);
		return 0;
	}

	ret = tegra_init_shared_bus_floor(&tegra21_emc_floor_sysfs,
					  1, emc_kobj);
	if (ret) {
		pr_err("tegra21_dvfs: failed to init emc floor interface (%d)\n",
		       ret);
		kobject_del(emc_kobj);
		return 0;
	}

	pr_info("tegra dvfs: tegra sysfs gpu & emc interface is initialized\n");

	return 0;
}
late_initcall(tegra21_dvfs_init_core_cap);
