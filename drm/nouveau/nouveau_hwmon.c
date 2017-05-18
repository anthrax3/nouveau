/*
 * Copyright 2010 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */

#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#endif
#include <linux/power_supply.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include <drm/drmP.h>

#include "nouveau_drv.h"
#include "nouveau_hwmon.h"

#include <nvkm/subdev/iccsense.h>
#include <nvkm/subdev/volt.h>

#if defined(CONFIG_HWMON) || (defined(MODULE) && defined(CONFIG_HWMON_MODULE))
static ssize_t
nouveau_hwmon_show_temp(struct device *d, struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	int temp = nvkm_therm_temp_get(therm);

	if (temp < 0)
		return temp;

	return snprintf(buf, PAGE_SIZE, "%d\n", temp * 1000);
}
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, nouveau_hwmon_show_temp,
						  NULL, 0);

static ssize_t
nouveau_hwmon_show_temp1_auto_point1_pwm(struct device *d,
					 struct device_attribute *a, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 100);
}
static SENSOR_DEVICE_ATTR(temp1_auto_point1_pwm, S_IRUGO,
			  nouveau_hwmon_show_temp1_auto_point1_pwm, NULL, 0);

static ssize_t
nouveau_hwmon_temp1_auto_point1_temp(struct device *d,
				     struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	      therm->attr_get(therm, NVKM_THERM_ATTR_THRS_FAN_BOOST) * 1000);
}
static ssize_t
nouveau_hwmon_set_temp1_auto_point1_temp(struct device *d,
					 struct device_attribute *a,
					 const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_FAN_BOOST,
			value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_auto_point1_temp, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_temp1_auto_point1_temp,
			  nouveau_hwmon_set_temp1_auto_point1_temp, 0);

static ssize_t
nouveau_hwmon_temp1_auto_point1_temp_hyst(struct device *d,
					  struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	 therm->attr_get(therm, NVKM_THERM_ATTR_THRS_FAN_BOOST_HYST) * 1000);
}
static ssize_t
nouveau_hwmon_set_temp1_auto_point1_temp_hyst(struct device *d,
					      struct device_attribute *a,
					      const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_FAN_BOOST_HYST,
			value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_temp1_auto_point1_temp_hyst,
			  nouveau_hwmon_set_temp1_auto_point1_temp_hyst, 0);

static ssize_t
nouveau_hwmon_max_temp(struct device *d, struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	       therm->attr_get(therm, NVKM_THERM_ATTR_THRS_DOWN_CLK) * 1000);
}
static ssize_t
nouveau_hwmon_set_max_temp(struct device *d, struct device_attribute *a,
						const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_DOWN_CLK, value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_max, S_IRUGO | S_IWUSR, nouveau_hwmon_max_temp,
						  nouveau_hwmon_set_max_temp,
						  0);

static ssize_t
nouveau_hwmon_max_temp_hyst(struct device *d, struct device_attribute *a,
			    char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	  therm->attr_get(therm, NVKM_THERM_ATTR_THRS_DOWN_CLK_HYST) * 1000);
}
static ssize_t
nouveau_hwmon_set_max_temp_hyst(struct device *d, struct device_attribute *a,
						const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_DOWN_CLK_HYST,
			value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_max_hyst, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_max_temp_hyst,
			  nouveau_hwmon_set_max_temp_hyst, 0);

static ssize_t
nouveau_hwmon_critical_temp(struct device *d, struct device_attribute *a,
							char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	       therm->attr_get(therm, NVKM_THERM_ATTR_THRS_CRITICAL) * 1000);
}
static ssize_t
nouveau_hwmon_set_critical_temp(struct device *d, struct device_attribute *a,
							    const char *buf,
								size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_CRITICAL, value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_crit, S_IRUGO | S_IWUSR,
						nouveau_hwmon_critical_temp,
						nouveau_hwmon_set_critical_temp,
						0);

static ssize_t
nouveau_hwmon_critical_temp_hyst(struct device *d, struct device_attribute *a,
							char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	  therm->attr_get(therm, NVKM_THERM_ATTR_THRS_CRITICAL_HYST) * 1000);
}
static ssize_t
nouveau_hwmon_set_critical_temp_hyst(struct device *d,
				     struct device_attribute *a,
				     const char *buf,
				     size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_CRITICAL_HYST,
			value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_crit_hyst, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_critical_temp_hyst,
			  nouveau_hwmon_set_critical_temp_hyst, 0);
static ssize_t
nouveau_hwmon_emergency_temp(struct device *d, struct device_attribute *a,
							char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	       therm->attr_get(therm, NVKM_THERM_ATTR_THRS_SHUTDOWN) * 1000);
}
static ssize_t
nouveau_hwmon_set_emergency_temp(struct device *d, struct device_attribute *a,
							    const char *buf,
								size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_SHUTDOWN, value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_emergency, S_IRUGO | S_IWUSR,
					nouveau_hwmon_emergency_temp,
					nouveau_hwmon_set_emergency_temp,
					0);

static ssize_t
nouveau_hwmon_emergency_temp_hyst(struct device *d, struct device_attribute *a,
							char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n",
	  therm->attr_get(therm, NVKM_THERM_ATTR_THRS_SHUTDOWN_HYST) * 1000);
}
static ssize_t
nouveau_hwmon_set_emergency_temp_hyst(struct device *d,
				      struct device_attribute *a,
				      const char *buf,
				      size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return count;

	therm->attr_set(therm, NVKM_THERM_ATTR_THRS_SHUTDOWN_HYST,
			value / 1000);

	return count;
}
static SENSOR_DEVICE_ATTR(temp1_emergency_hyst, S_IRUGO | S_IWUSR,
					nouveau_hwmon_emergency_temp_hyst,
					nouveau_hwmon_set_emergency_temp_hyst,
					0);

static ssize_t nouveau_hwmon_show_name(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "nouveau\n");
}
static SENSOR_DEVICE_ATTR(name, S_IRUGO, nouveau_hwmon_show_name, NULL, 0);

static ssize_t nouveau_hwmon_show_update_rate(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "1000\n");
}
static SENSOR_DEVICE_ATTR(update_rate, S_IRUGO,
						nouveau_hwmon_show_update_rate,
						NULL, 0);

static ssize_t
nouveau_hwmon_show_fan1_input(struct device *d, struct device_attribute *attr,
			      char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	return snprintf(buf, PAGE_SIZE, "%d\n", nvkm_therm_fan_sense(therm));
}
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, nouveau_hwmon_show_fan1_input,
			  NULL, 0);

 static ssize_t
nouveau_hwmon_get_pwm1_enable(struct device *d,
			   struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	int ret;

	ret = therm->attr_get(therm, NVKM_THERM_ATTR_FAN_MODE);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%i\n", ret);
}

static ssize_t
nouveau_hwmon_set_pwm1_enable(struct device *d, struct device_attribute *a,
			   const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;
	int ret;

	ret = kstrtol(buf, 10, &value);
	if (ret)
		return ret;

	ret = therm->attr_set(therm, NVKM_THERM_ATTR_FAN_MODE, value);
	if (ret)
		return ret;
	else
		return count;
}
static SENSOR_DEVICE_ATTR(pwm1_enable, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_get_pwm1_enable,
			  nouveau_hwmon_set_pwm1_enable, 0);

static ssize_t
nouveau_hwmon_get_pwm1(struct device *d, struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	int ret;

	ret = therm->fan_get(therm);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%i\n", ret);
}

static ssize_t
nouveau_hwmon_set_pwm1(struct device *d, struct device_attribute *a,
		       const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	int ret = -ENODEV;
	long value;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return -EINVAL;

	ret = therm->fan_set(therm, value);
	if (ret)
		return ret;

	return count;
}

static SENSOR_DEVICE_ATTR(pwm1, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_get_pwm1,
			  nouveau_hwmon_set_pwm1, 0);

static ssize_t
nouveau_hwmon_get_pwm1_min(struct device *d,
			   struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	int ret;

	ret = therm->attr_get(therm, NVKM_THERM_ATTR_FAN_MIN_DUTY);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%i\n", ret);
}

static ssize_t
nouveau_hwmon_set_pwm1_min(struct device *d, struct device_attribute *a,
			   const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;
	int ret;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return -EINVAL;

	ret = therm->attr_set(therm, NVKM_THERM_ATTR_FAN_MIN_DUTY, value);
	if (ret < 0)
		return ret;

	return count;
}

static SENSOR_DEVICE_ATTR(pwm1_min, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_get_pwm1_min,
			  nouveau_hwmon_set_pwm1_min, 0);

static ssize_t
nouveau_hwmon_get_pwm1_max(struct device *d,
			   struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	int ret;

	ret = therm->attr_get(therm, NVKM_THERM_ATTR_FAN_MAX_DUTY);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%i\n", ret);
}

static ssize_t
nouveau_hwmon_set_pwm1_max(struct device *d, struct device_attribute *a,
			   const char *buf, size_t count)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	long value;
	int ret;

	if (kstrtol(buf, 10, &value) == -EINVAL)
		return -EINVAL;

	ret = therm->attr_set(therm, NVKM_THERM_ATTR_FAN_MAX_DUTY, value);
	if (ret < 0)
		return ret;

	return count;
}

static SENSOR_DEVICE_ATTR(pwm1_max, S_IRUGO | S_IWUSR,
			  nouveau_hwmon_get_pwm1_max,
			  nouveau_hwmon_set_pwm1_max, 0);

static ssize_t
nouveau_hwmon_get_in0_input(struct device *d,
			    struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_volt *volt = nvxx_volt(&drm->client.device);
	int ret;

	ret = nvkm_volt_get(volt);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%i\n", ret / 1000);
}

static SENSOR_DEVICE_ATTR(in0_input, S_IRUGO,
			  nouveau_hwmon_get_in0_input, NULL, 0);

static ssize_t
nouveau_hwmon_get_in0_min(struct device *d,
			    struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_volt *volt = nvxx_volt(&drm->client.device);

	if (!volt || !volt->min_uv)
		return -ENODEV;

	return sprintf(buf, "%i\n", volt->min_uv / 1000);
}

static SENSOR_DEVICE_ATTR(in0_min, S_IRUGO,
			  nouveau_hwmon_get_in0_min, NULL, 0);

static ssize_t
nouveau_hwmon_get_in0_max(struct device *d,
			    struct device_attribute *a, char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_volt *volt = nvxx_volt(&drm->client.device);

	if (!volt || !volt->max_uv)
		return -ENODEV;

	return sprintf(buf, "%i\n", volt->max_uv / 1000);
}

static SENSOR_DEVICE_ATTR(in0_max, S_IRUGO,
			  nouveau_hwmon_get_in0_max, NULL, 0);

static ssize_t
nouveau_hwmon_get_in0_label(struct device *d,
			    struct device_attribute *a, char *buf)
{
	return sprintf(buf, "GPU core\n");
}

static SENSOR_DEVICE_ATTR(in0_label, S_IRUGO,
			  nouveau_hwmon_get_in0_label, NULL, 0);

static ssize_t
nouveau_hwmon_get_power1_input(struct device *d, struct device_attribute *a,
			       char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_iccsense *iccsense = nvxx_iccsense(&drm->client.device);
	int result = nvkm_iccsense_read_all(iccsense);

	if (result < 0)
		return result;

	return sprintf(buf, "%i\n", result);
}

static SENSOR_DEVICE_ATTR(power1_input, S_IRUGO,
			  nouveau_hwmon_get_power1_input, NULL, 0);

static ssize_t
nouveau_hwmon_get_power1_max(struct device *d, struct device_attribute *a,
			     char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_iccsense *iccsense = nvxx_iccsense(&drm->client.device);
	return sprintf(buf, "%i\n", iccsense->power_w_max);
}

static SENSOR_DEVICE_ATTR(power1_max, S_IRUGO,
			  nouveau_hwmon_get_power1_max, NULL, 0);

static ssize_t
nouveau_hwmon_get_power1_crit(struct device *d, struct device_attribute *a,
			      char *buf)
{
	struct drm_device *dev = dev_get_drvdata(d);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_iccsense *iccsense = nvxx_iccsense(&drm->client.device);
	return sprintf(buf, "%i\n", iccsense->power_w_crit);
}

static SENSOR_DEVICE_ATTR(power1_crit, S_IRUGO,
			  nouveau_hwmon_get_power1_crit, NULL, 0);

static struct attribute *hwmon_default_attributes[] = {
	&sensor_dev_attr_name.dev_attr.attr,
	&sensor_dev_attr_update_rate.dev_attr.attr,
	NULL
};
static struct attribute *hwmon_temp_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_auto_point1_pwm.dev_attr.attr,
	&sensor_dev_attr_temp1_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_temp1_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	&sensor_dev_attr_temp1_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_emergency.dev_attr.attr,
	&sensor_dev_attr_temp1_emergency_hyst.dev_attr.attr,
	NULL
};
static struct attribute *hwmon_fan_rpm_attributes[] = {
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	NULL
};
static struct attribute *hwmon_pwm_fan_attributes[] = {
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_min.dev_attr.attr,
	&sensor_dev_attr_pwm1_max.dev_attr.attr,
	NULL
};

static struct attribute *hwmon_in0_attributes[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
	NULL
};

static struct attribute *hwmon_power_attributes[] = {
	&sensor_dev_attr_power1_input.dev_attr.attr,
	NULL
};

static struct attribute *hwmon_power_caps_attributes[] = {
	&sensor_dev_attr_power1_max.dev_attr.attr,
	&sensor_dev_attr_power1_crit.dev_attr.attr,
	NULL
};

static const struct attribute_group hwmon_default_attrgroup = {
	.attrs = hwmon_default_attributes,
};
static const struct attribute_group hwmon_temp_attrgroup = {
	.attrs = hwmon_temp_attributes,
};
static const struct attribute_group hwmon_fan_rpm_attrgroup = {
	.attrs = hwmon_fan_rpm_attributes,
};
static const struct attribute_group hwmon_pwm_fan_attrgroup = {
	.attrs = hwmon_pwm_fan_attributes,
};
static const struct attribute_group hwmon_in0_attrgroup = {
	.attrs = hwmon_in0_attributes,
};
static const struct attribute_group hwmon_power_attrgroup = {
	.attrs = hwmon_power_attributes,
};
static const struct attribute_group hwmon_power_caps_attrgroup = {
	.attrs = hwmon_power_caps_attributes,
};

static const u32 nouveau_config_chip[] = {
	HWMON_C_UPDATE_INTERVAL,
	0
};

static const u32 nouveau_config_in[] = {
	HWMON_I_INPUT | HWMON_I_MIN | HWMON_I_MAX | HWMON_I_LABEL,
	0
};

static const u32 nouveau_config_temp[] = {
	HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_MAX_HYST |
	HWMON_T_CRIT | HWMON_T_CRIT_HYST | HWMON_T_EMERGENCY |
	HWMON_T_EMERGENCY_HYST,
	0
};

static const u32 nouveau_config_fan[] = {
	HWMON_F_INPUT,
	0
};

static const u32 nouveau_config_pwm[] = {
	HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
	0
};

static const u32 nouveau_config_power[] = {
	HWMON_P_INPUT | HWMON_P_CAP_MAX | HWMON_P_CRIT,
	0
};

static const struct hwmon_channel_info nouveau_chip = {
	.type = hwmon_chip,
	.config = nouveau_config_chip,
};

static const struct hwmon_channel_info nouveau_temp = {
	.type = hwmon_temp,
	.config = nouveau_config_temp,
};

static const struct hwmon_channel_info nouveau_fan = {
	.type = hwmon_fan,
	.config = nouveau_config_fan,
};

static const struct hwmon_channel_info nouveau_in = {
	.type = hwmon_in,
	.config = nouveau_config_in,
};

static const struct hwmon_channel_info nouveau_pwm = {
	.type = hwmon_pwm,
	.config = nouveau_config_pwm,
};

static const struct hwmon_channel_info nouveau_power = {
	.type = hwmon_power,
	.config = nouveau_config_power,
};

static const struct hwmon_channel_info *nouveau_info[] = {
	&nouveau_chip,
	&nouveau_temp,
	&nouveau_fan,
	&nouveau_in,
	&nouveau_pwm,
	&nouveau_power,
	NULL
};

static umode_t
nouveau_chip_is_visible(const void *data, u32 attr, int channel)
{
	switch (attr) {
	case hwmon_chip_update_interval:
		return 0444;
	default:
		return 0;
	}
}

static umode_t
nouveau_power_is_visible(const void *data, u32 attr, int channel)
{
	struct nouveau_drm *drm = nouveau_drm((struct drm_device *)data);
	struct nvkm_iccsense *iccsense = nvxx_iccsense(&drm->client.device);

	if (!iccsense || !iccsense->data_valid || list_empty(&iccsense->rails))
		return 0;

	switch (attr) {
	case hwmon_power_input:
		return 0444;
	case hwmon_power_max:
		if (iccsense->power_w_max)
			return 0444;
		return 0;
	case hwmon_power_crit:
		if (iccsense->power_w_crit)
			return 0444;
		return 0;
	default:
		return 0;
	}
}

static umode_t
nouveau_temp_is_visible(const void *data, u32 attr, int channel)
{
	struct nouveau_drm *drm = nouveau_drm((struct drm_device *)data);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	if (therm && therm->attr_get && nvkm_therm_temp_get(therm) < 0)
		return 0;

	switch (attr) {
	case hwmon_temp_input:
	case hwmon_temp_max:
	case hwmon_temp_max_hyst:
	case hwmon_temp_crit:
	case hwmon_temp_crit_hyst:
	case hwmon_temp_emergency:
	case hwmon_temp_emergency_hyst:
		return 0444;
	default:
		return 0;
	}
}

static umode_t
nouveau_pwm_is_visible(const void *data, u32 attr, int channel)
{
	struct nouveau_drm *drm = nouveau_drm((struct drm_device *)data);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	if (therm && therm->attr_get && therm->fan_get &&
				therm->fan_get(therm) < 0)
		return 0;

	switch (attr) {
	case hwmon_pwm_enable:
	case hwmon_pwm_input:
		return 0644;
	default:
		return 0;
	}
}

static umode_t
nouveau_input_is_visible(const void *data, u32 attr, int channel)
{
	struct nouveau_drm *drm = nouveau_drm((struct drm_device *)data);
	struct nvkm_volt *volt = nvxx_volt(&drm->client.device);

	if (!volt || nvkm_volt_get(volt) < 0)
		return 0;

	switch (attr) {
	case hwmon_in_input:
	case hwmon_in_label:
	case hwmon_in_min:
	case hwmon_in_max:
		return 0444;
	default:
		return 0;
	}
}

static umode_t
nouveau_fan_is_visible(const void *data, u32 attr, int channel)
{
	struct nouveau_drm *drm = nouveau_drm((struct drm_device *)data);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);

	if (!therm || !therm->attr_get || nvkm_therm_fan_sense(therm) < 0)
		return 0;

	switch (attr) {
	case hwmon_fan_input:
		return 0444;
	default:
		return 0;
	}
}

static umode_t
nouveau_is_visible(const void *data, enum hwmon_sensor_types type, u32 attr,
			int channel)
{
	switch (type) {
	case hwmon_chip:
		return nouveau_chip_is_visible(data, attr, channel);
	case hwmon_temp:
		return nouveau_temp_is_visible(data, attr, channel);
	case hwmon_fan:
		return nouveau_fan_is_visible(data, attr, channel);
	case hwmon_in:
		return nouveau_input_is_visible(data, attr, channel);
	case hwmon_pwm:
		return nouveau_pwm_is_visible(data, attr, channel);
	case hwmon_power:
		return nouveau_power_is_visible(data, attr, channel);
	default:
		return 0;
	}
}

static const char input_label[] = "GPU core";

static int
nouveau_read_string(struct device *dev, enum hwmon_sensor_types type, u32 attr,
		    int channel, const char **buf)
{
	if (type == hwmon_in && attr == hwmon_in_label) {
		*buf = input_label;
		return 0;
	}

	return -EOPNOTSUPP;
}

static const struct hwmon_ops nouveau_hwmon_ops = {
	.is_visible = nouveau_is_visible,
	.read = NULL,
	.read_string = nouveau_read_string,
	.write = NULL,
};

static const struct hwmon_chip_info nouveau_chip_info = {
	.ops = &nouveau_hwmon_ops,
	.info = nouveau_info,
};
#endif

int
nouveau_hwmon_init(struct drm_device *dev)
{
#if defined(CONFIG_HWMON) || (defined(MODULE) && defined(CONFIG_HWMON_MODULE))
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_therm *therm = nvxx_therm(&drm->client.device);
	struct nvkm_volt *volt = nvxx_volt(&drm->client.device);
	struct nvkm_iccsense *iccsense = nvxx_iccsense(&drm->client.device);
	struct nouveau_hwmon *hwmon;
	struct device *hwmon_dev;
	int ret = 0;

	hwmon = drm->hwmon = kzalloc(sizeof(*hwmon), GFP_KERNEL);
	if (!hwmon)
		return -ENOMEM;
	hwmon->dev = dev;

	hwmon_dev = hwmon_device_register(dev->dev);
	if (IS_ERR(hwmon_dev)) {
		ret = PTR_ERR(hwmon_dev);
		NV_ERROR(drm, "Unable to register hwmon device: %d\n", ret);
		return ret;
	}
	dev_set_drvdata(hwmon_dev, dev);

	/* set the default attributes */
	ret = sysfs_create_group(&hwmon_dev->kobj, &hwmon_default_attrgroup);
	if (ret)
		goto error;

	if (therm && therm->attr_get && therm->attr_set) {
		/* if the card has a working thermal sensor */
		if (nvkm_therm_temp_get(therm) >= 0) {
			ret = sysfs_create_group(&hwmon_dev->kobj, &hwmon_temp_attrgroup);
			if (ret)
				goto error;
		}

		/* if the card has a pwm fan */
		/*XXX: incorrect, need better detection for this, some boards have
		 *     the gpio entries for pwm fan control even when there's no
		 *     actual fan connected to it... therm table? */
		if (therm->fan_get && therm->fan_get(therm) >= 0) {
			ret = sysfs_create_group(&hwmon_dev->kobj,
						 &hwmon_pwm_fan_attrgroup);
			if (ret)
				goto error;
		}
	}

	/* if the card can read the fan rpm */
	if (therm && nvkm_therm_fan_sense(therm) >= 0) {
		ret = sysfs_create_group(&hwmon_dev->kobj,
					 &hwmon_fan_rpm_attrgroup);
		if (ret)
			goto error;
	}

	if (volt && nvkm_volt_get(volt) >= 0) {
		ret = sysfs_create_group(&hwmon_dev->kobj,
					 &hwmon_in0_attrgroup);

		if (ret)
			goto error;
	}

	if (iccsense && iccsense->data_valid && !list_empty(&iccsense->rails)) {
		ret = sysfs_create_group(&hwmon_dev->kobj,
					 &hwmon_power_attrgroup);

		if (ret)
			goto error;

		if (iccsense->power_w_max && iccsense->power_w_crit) {
			ret = sysfs_create_group(&hwmon_dev->kobj,
						 &hwmon_power_caps_attrgroup);
			if (ret)
				goto error;
		}
	}

	hwmon->hwmon = hwmon_dev;

	return 0;

error:
	NV_ERROR(drm, "Unable to create some hwmon sysfs files: %d\n", ret);
	hwmon_device_unregister(hwmon_dev);
	hwmon->hwmon = NULL;
	return ret;
#else
	return 0;
#endif
}

void
nouveau_hwmon_fini(struct drm_device *dev)
{
#if defined(CONFIG_HWMON) || (defined(MODULE) && defined(CONFIG_HWMON_MODULE))
	struct nouveau_hwmon *hwmon = nouveau_hwmon(dev);

	if (hwmon->hwmon) {
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_default_attrgroup);
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_temp_attrgroup);
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_pwm_fan_attrgroup);
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_fan_rpm_attrgroup);
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_in0_attrgroup);
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_power_attrgroup);
		sysfs_remove_group(&hwmon->hwmon->kobj, &hwmon_power_caps_attrgroup);

		hwmon_device_unregister(hwmon->hwmon);
	}

	nouveau_drm(dev)->hwmon = NULL;
	kfree(hwmon);
#endif
}
