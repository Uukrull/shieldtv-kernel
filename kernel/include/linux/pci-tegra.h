/*
 *  Header file contains constants and structures for tegra PCIe driver.
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __PCI_TEGRA_H
#define __PCI_TEGRA_H

#include <linux/pci.h>

#define MAX_PCIE_SUPPORTED_PORTS 2

struct tegra_pci_platform_data {
	bool has_memtype_lpddr4; /* apply WAR for lpddr4 mem */
	int gpio_hot_plug; /* GPIO num to support hotplug */
	int gpio_wake; /* GPIO num to support WAKE from LP0 */
	int gpio_x1_slot; /* GPIO num to enable x1 slot */
	u32 lane_map; /* lane mux info in byte nibbles */
};
#endif
