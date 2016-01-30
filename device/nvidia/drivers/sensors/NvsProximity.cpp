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

/* The NVS = NVidia Sensor framework */
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
 *    The values are in the IIO IIO_VAL_INT_PLUS_MICRO format.
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
 * allow a window around the last reported HW value.  For example, say the low
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


#include "NvsProximity.h"


NvsProximity::NvsProximity(int devNum, enum NVS_DRIVER_TYPE driverType, Nvs *link)
    : Nvs(devNum,
          "proximity",
          SENSOR_TYPE_PROXIMITY,
          driverType,
          link)
{
}

NvsProximity::~NvsProximity()
{
}

