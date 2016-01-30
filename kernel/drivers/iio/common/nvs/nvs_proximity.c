/* Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* The NVS = NVidia Sensor framework */
/* This common NVS proximity module allows, along with the NVS IIO common
 * module, a proximity driver to offload the code interacting with IIO and
 * proximity reporting, and just have code that interacts with the HW.
 * The commonality between this module and the NVS ALS driver is the
 * nvs_proximity structure.  It is expected that the NVS proximity driver will:
 * - call nvs_proximity_enable when the device is enabled for initialization.
 * - read the HW and place the value in nvs_proximity.hw
 * - call nvs_proximity_read
 * - depending on the nvs_proximity_read return value:
 *     - -1 = poll HW using nvs_proximity.poll_delay_ms delay.
 *     - 0 = if interrupt driven, do nothing or resume regular polling
 *     - 1 = set new thresholds using the nvs_proximity.hw_thresh_lo/hi
 * Reporting the distance is handled within this module.
 * See nvs_proximity.h for nvs_proximity structure details.
 */
/* NVS proximity drivers can be configured for binary output.  If the max_range
 * and resolution settings in the device tree is set for 1.0, the driver
 * will configure the rest of the settings so that a 1 is reported for
 * "far away" and 0 for "near".  The low threshold is typically set for maximum
 * range allowing the minimal LED drive power to determine the actual range.
 * If proximity binary output is disabled, the driver will then require the
 * interpolation calibration for reporting actual distances.
 */
/* The NVS HAL will use the IIO scale and offset sysfs attributes to modify the
 * data using the following formula: (data * scale) + offset
 * A scale value of 0 disables scale.
 * A scale value of 1 puts the NVS HAL into calibration mode where the scale
 * and offset are read everytime the data is read to allow realtime calibration
 * of the scale and offset values to be used in the device tree parameters.
 * Keep in mind the data is buffered but the NVS HAL will display the data and
 * scale/offset parameters in the log.  See calibration steps below.
 */
/* Because the proximity HW can use dynamic resolution depending on the
 * distance range, configuration threshold values are HW based.  In other
 * words, the threshold will automatically scale based on the resolution.
 */
/* If the NVS proximity driver is not configured for binary output, then
 * there are two calibration mechanisms that can be used:
 * Method 1 (preferred):
 * This method uses interpolation and requires a low and high uncalibrated
 * value along with the corresponding low and high calibrated values.  The
 * uncalibrated values are what is read from the sensor in the steps below.
 * The corresponding calibrated values are what the correct value should be.
 * All values are programmed into the device tree settings.
 * 1. Read scale sysfs attribute.  This value will need to be written back.
 * 2. Disable device.
 * 3. Write 1 to the scale sysfs attribute.
 * 4. Enable device.
 * 5. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 6. Write the scale value read in step 1 back to the scale sysfs attribute.
 * 7. Put the device into a state where the data read is a low value.
 * 8. Note the values displayed in the log.  Separately measure the actual
 *    value.  The value from the sensor will be the uncalibrated value and the
 *    separately measured value will be the calibrated value for the current
 *    state (low or high values).
 * 9. Put the device into a state where the data read is a high value.
 * 10. Repeat step 8.
 * 11. Enter the values in the device tree settings for the device.  Both
 *     calibrated and uncalibrated values will be the values before scale and
 *     offset are applied.
 *     The proximity sensor has the following device tree parameters for this:
 *     proximity_uncalibrated_lo
 *     proximity_calibrated_lo
 *     proximity_uncalibrated_hi
 *     proximity_calibrated_hi
 *
 * Method 2:
 * 1. Disable device.
 * 2. Write 1 to the scale sysfs attribute.
 * 3. Enable device.
 * 4. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 5. Write to scale and offset sysfs attributes as needed to get the data
 *    modified as desired.
 * 6. Disabling the device disables calibration mode.
 * 7. Set the new scale and offset parameters in the device tree:
 *    proximity_scale_ival = the integer value of the scale.
 *    proximity_scale_fval = the floating value of the scale.
 *    proximity_offset_ival = the integer value of the offset.
 *    proximity_offset_fval = the floating value of the offset.
 *    The values are in the NVS_FLOAT_SIGNIFICANCE_ format (see nvs.h).
 */
/* The reason calibration method 1 is preferred is that the NVS proximity
 * driver already sets the scaling to coordinate with the resolution by
 * multiplying the HW data value read with resolution * scaling and then
 * divides it back down with the scaling so that no significance is lost.
 */
/* If the NVS proximity driver is configured for binary output, then
 * interpolation is not used and the thresholds are used to trigger either the
 * 0 or 1 output.  To calibrate the thresholds:
 * 1. Disable device.
 * 2. Write 1 to the scale sysfs attribute.
 * 3. Enable device.
 * 4. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the HW proximity data.
 * 5. Move an object (your hand) through the proximity range.  Note the HW
 *    value when the object is at a point that the output should be 0.  This
 *    will be the high threshold value.  Move the object away from the sensor
 *    and note the HW value where the output should change to 1.  This will be
 *    the low threshold value.
 * NOTE: Proximity typically works by reading the reflected IR light from an
 *       LED.  The more light reflected, the higher the HW value and the closer
 *       the object is.  Because of this, the thresholds appear to be reversed
 *       to the output, but keep in mind, the thresholds are HW based, so low
 *       threshold means low HW value regardless of the actual output.
 * NOTE: If greater range is needed, modify the LED output strength if the
 *       proximity HW supports it.  This will be a DT configuration option that
 *       is specific to the driver and HW.
 * 6. Enter the threshold values in the device tree settings for the device.
 *     The proximity sensor has the following device tree parameters for this:
 *     proximity_threshold_lo
 *     proximity_threshold_hi
 */
/* If the NVS proximity driver is not configured for binary output, then the
 * thresholds are used for hysterysis.  The threshold settings are HW based and
 * allow a window around the last reported HW value.  For example, if the low
 * threshold is set to 10 and the high threshold set to 20, if the proximity HW
 * value is 100, the proximity won't be reported again until the proximity HW
 * value is either < 90 or > than 120.
 * The low/high threshold values are typically the same, but they can be
 * configured so that proximity changes at a different rate based on the
 * direction of change.
 * Use the calibration methods for a steady output of data to get an idea of
 * the debounce desired.
 * NOTE: If both configuration thresholds are 0, then thresholds are disabled.
 * NOTE: An NVS feature is the use of the report_count configuration variable,
 *       proximity_report_count in DT (see nvs.h).  This allows additional
 *       reporting of proximity a set amount of times while still within the
 *       threshold window.
 */
/* If the NVS proximity driver is configured for binary output, then the
 * thresholds are absolute HW values.  If not configured for binary output,
 * then the thresholds are relative HW values to set a trigger window around
 * the last read HW value.
 */


#include <linux/nvs_proximity.h>


static void nvs_proximity_interpolate(int x1, s64 x2, int x3,
				      int y1, u32 *y2, int y3)
{
	s64 dividend;
	s64 divisor;

	/* y2 = ((x2 - x1)(y3 - y1)/(x3 - x1)) + y1 */
	divisor = (x3 - x1);
	if (!divisor) {
		*y2 = (u32)x2;
		return;
	}

	dividend = (x2 - x1) * (y3 - y1);
	do_div(dividend, divisor);
	dividend += y1;
	if (dividend < 0)
		dividend = 0;
	*y2 = (u32)dividend;
}

/**
 * nvs_proximity_read - called after HW is read and written to
 *                      np.
 * @np: the common structure between driver and common module.
 *
 * This will handle the conversion of HW to distance value,
 * reporting, calculation of thresholds and poll time.
 *
 * Returns: -1 = Error and/or polling is required for next
 *               sample regardless of being interrupt driven.
 *          0 = Do nothing.  Value has not changed for reporting
 *              and same threshold values if interrupt driven.
 *              If not interrupt driven use poll_delay_ms.
 *          1 = New HW thresholds are needed.
 *              If not interrupt driven use poll_delay_ms.
 */
int nvs_proximity_read(struct nvs_proximity *np)
{
	u64 calc_i;
	u64 calc_f;
	s64 calc;
	s64 timestamp_diff;
	s64 delay;
	bool report_delay_min = true;
	unsigned int poll_delay = 0;
	unsigned int thresh_lo;
	unsigned int thresh_hi;
	unsigned int hw_distance;
	int ret;

	if (np->calibration_en)
		/* always report without report_delay_min */
		np->report = np->cfg->report_n;
	if (np->report < np->cfg->report_n) { /* always report first sample */
		/* calculate elapsed time for allowed report rate */
		timestamp_diff = np->timestamp - np->timestamp_report;
		delay = np->delay_us * 1000;
		if (timestamp_diff < delay) {
			/* data changes are happening faster than allowed to
			 * report so we poll for the next data at an allowed
			 * rate with interrupts disabled.
			 */
			delay -= timestamp_diff;
			do_div(delay, 1000); /* ns => us */
			poll_delay = delay;
			report_delay_min = false;
		}
	}
	/* threshold flags */
	thresh_lo = np->cfg->thresh_lo;
	thresh_hi = np->cfg->thresh_hi;
	if (thresh_lo < np->hw_mask) {
		np->thresh_valid_lo = true;
	} else {
		np->thresh_valid_lo = false;
		thresh_lo = 0;
	}
	if (thresh_hi < np->hw_mask) {
		np->thresh_valid_hi = true;
	} else {
		np->thresh_valid_hi = false;
		thresh_hi = 0;
	}
	if (np->thresh_valid_lo && np->thresh_valid_hi)
		np->thresholds_valid = true;
	else
		np->thresholds_valid = false;
	/* limit flags */
	if ((np->hw < thresh_lo) || (np->hw == 0))
		np->hw_limit_lo = true;
	else
		np->hw_limit_lo = false;
	if (np->proximity_binary_en) {
		if (np->hw > thresh_hi)
			np->hw_limit_hi = true;
		else
			np->hw_limit_hi = false;
	} else {
		if ((np->hw == np->hw_mask) || (np->hw >
						(np->hw_mask - thresh_hi)))
			np->hw_limit_hi = true;
		else
			np->hw_limit_hi = false;
	}
	ret = RET_NO_CHANGE;
	if (np->proximity_binary_en) {
		/* proximity has binary threshold */
		if (!np->thresholds_valid) {
			/* Invalid thresholds is an NVS feature that forces
			 * polling.  However, with this binary mechanism,
			 * thresholds are required.  So although the feature
			 * is somewhat crippled, we make it work by setting
			 * the trigger in the middle of the HW range.
			 */
			thresh_lo = np->hw_mask / 2;
			thresh_hi = thresh_lo;
			np->report = np->cfg->report_n;
		}
		if (np->hw < np->hw_thresh_lo) {
			np->proximity = 1;
			np->report = np->cfg->report_n;
			/* disable lower threshold */
			np->hw_thresh_lo = 0;
			/* enable upper threshold */
			np->hw_thresh_hi = thresh_hi;
		} else if (np->hw > np->hw_thresh_hi) {
			np->proximity = 0;
			np->report = np->cfg->report_n;
			/* disable upper threshold */
			np->hw_thresh_hi = np->hw_mask;
			/* enable lower threshold */
			np->hw_thresh_lo = thresh_lo;
		}
		if (np->calibration_en)
			np->proximity = np->hw;
		if (np->report && report_delay_min) {
			np->report--;
			np->timestamp_report = np->timestamp;
			np->handler(np->nvs_data, &np->proximity,
				    np->timestamp_report);
			ret = RET_HW_UPDATE;
		}
	} else {
		/* reporting and thresholds */
		if (np->thresholds_valid) {
			if (np->hw < np->hw_thresh_lo)
				np->report = np->cfg->report_n;
			else if (np->hw > np->hw_thresh_hi)
				np->report = np->cfg->report_n;
		} else {
			/* report everything if no thresholds */
			np->report = np->cfg->report_n;
		}
		if (np->report && report_delay_min) {
			np->report--;
			np->timestamp_report = np->timestamp;
			/* reverse the value in the range */
			hw_distance = np->hw_mask - np->hw;
			/* distance = HW * (resolution *
			 *                  NVS_FLOAT_SIGNIFICANCE_) / scale
			 */
			calc_f = 0;
			if (np->cfg->resolution.fval) {
				calc_f = (u64)(hw_distance *
					       np->cfg->resolution.fval);
				if (np->cfg->scale.fval)
					do_div(calc_f, np->cfg->scale.fval);
			}
			calc_i = 0;
			if (np->cfg->resolution.ival) {
				if (np->cfg->float_significance)
					calc_i = NVS_FLOAT_SIGNIFICANCE_NANO /
							   np->cfg->scale.fval;
				else
					calc_i = NVS_FLOAT_SIGNIFICANCE_MICRO /
							   np->cfg->scale.fval;
				calc_i *= (u64)(hw_distance *
						np->cfg->resolution.ival);
			}
			calc = (s64)(calc_i + calc_f);
			if (np->calibration_en) {
				/* when in calibration mode just return calc */
				np->proximity = (u32)calc;
			} else {
				/* get calibrated value */
				nvs_proximity_interpolate(np->cfg->uncal_lo,
							  calc,
							  np->cfg->uncal_hi,
							  np->cfg->cal_lo,
							  &np->proximity,
							  np->cfg->cal_hi);
			}
			/* report proximity */
			np->handler(np->nvs_data, &np->proximity,
				    np->timestamp_report);
			if ((np->thresholds_valid) && !np->report) {
				/* calculate low threshold */
				calc = (s64)np->hw;
				calc -= thresh_lo;
				if (calc < 0)
					/* low threshold is disabled */
					np->hw_thresh_lo = 0;
				else
					np->hw_thresh_lo = calc;
				/* calculate high threshold */
				calc = np->hw + thresh_hi;
				if (calc > np->hw_mask)
					/* high threshold is disabled */
					np->hw_thresh_hi = np->hw_mask;
				else
					np->hw_thresh_hi = calc;
				ret = RET_HW_UPDATE;
			}
		}
	}
	if (report_delay_min)
		poll_delay = np->delay_us;
	if ((poll_delay < np->cfg->delay_us_min) || np->calibration_en)
		poll_delay = np->cfg->delay_us_min;
	np->poll_delay_ms = poll_delay / 1000;
	if (np->report || np->calibration_en)
		ret = RET_POLL_NEXT; /* poll for next sample */
	return ret;
}

/**
 * nvs_proximity_enable - called when the proximity sensor is
 *                        enabled.
 * @np: the common structure between driver and common module.
 *
 * This inititializes the np NVS variables.
 *
 * Returns 0 on success or a negative error code.
 */
int nvs_proximity_enable(struct nvs_proximity *np)
{
	if (!np->cfg->report_n)
		np->cfg->report_n = 1;
	np->report = np->cfg->report_n;
	np->timestamp_report = 0;
	np->hw_thresh_hi = 0;
	np->hw_thresh_lo = -1;
	np->proximity = 1;
	if (np->cfg->resolution.ival == 1 && !np->cfg->resolution.fval &&
		      np->cfg->max_range.ival == 1 && !np->cfg->max_range.fval)
		np->proximity_binary_en = true;
	else
		np->proximity_binary_en = false;
	if (np->cfg->scale.ival == 1 && !np->cfg->scale.fval)
		np->calibration_en = true;
	else
		np->calibration_en = false;
	if (np->delay_us)
		np->poll_delay_ms = np->delay_us * 1000;
	else
		np->poll_delay_ms = np->cfg->delay_us_min * 1000;
	return 0;
}

/**
 * nvs_proximity_of_dt - called during system boot for
 * configuration from device tree.
 * @np: the common structure between driver and common module.
 * @dn: device node pointer.
 * @dev_name: device name string.  Typically a string to
 *            "proximity" or NULL.
 *
 * Returns 0 on success or a negative error code.
 *
 * Driver must initialize variables if no success.
 */
int nvs_proximity_of_dt(struct nvs_proximity *np, const struct device_node *dn,
			const char *dev_name)
{
	if (np->cfg)
		np->cfg->flags = SENSOR_FLAG_ON_CHANGE_MODE;
	return 0;
}

