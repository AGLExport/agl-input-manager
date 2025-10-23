/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-input.c
 * @brief	This file include device management functions using libudev.
 */
#include "device-input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <systemd/sd-event.h>
#include <list.h>
#include <libevdev/libevdev-uinput.h>
#include <errno.h>


static const char g_touch_device_name[] = "agl-input-touch";
static const int g_touch_device_max_width = 32767;
static const int g_touch_device_max_height = 32767;

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_device_input_abs_value {
	int valid;	/**< Value is valid(=1) or invalid(=0). */
	int value;	/**< The abs value. */
};
typedef struct s_device_input_abs_value device_input_abs_value_t;

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_device_input_key_value {
	int valid;	/**< Value is valid(=1) or invalid(=0). */
	int value;	/**< The key value. */
};
typedef struct s_device_input_key_value device_input_key_value_t;

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_device_input_timestamp_value {
	int valid;	/**< Value is valid(=1) or invalid(=0). */
	uint32_t value;	/**< The abs value. */
};
typedef struct s_device_input_timestamp_value device_input_timestamp_value_t;

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_device_input_abs_mt_element {
	device_input_abs_value_t slot;	/**< The slot value. */
	device_input_abs_value_t position_x;	/**< The position x value. */
	device_input_abs_value_t position_y;	/**< The position y value. */
	device_input_abs_value_t tracking_id;	/**< The tracking id value. */
};
typedef struct s_device_input_abs_mt_element device_input_abs_mt_element_t;

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_device_input_touch_data {
	size_t num_slots;	/**< The number of touch slots. */
	device_input_abs_mt_element_t *mt_elements;	/**< The array of MT element. */

	device_input_abs_value_t abs_x;	/**< The abs x value. */
	device_input_abs_value_t abs_y;	/**< The abs y value. */

	device_input_key_value_t btn_touch;	/**< The btn touch value. */

	device_input_timestamp_value_t timestamp;	/**< The timestamp value. */
};
typedef struct s_device_input_touch_data device_input_touch_data_t;

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_device_input {
	sd_event *event;
	struct libevdev *evdev;
	struct libevdev_uinput *uinput;

	struct dl_list devices;	/**< Double link list for input device management. */
};

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_touch_device {
	struct dl_list list;	/**< Double link list header. */
	struct s_device_input *pdi;	/**< Pointer to parent device input. */
	sd_event_source *input_event_source ;	/**< The sd event source for input device. */
	struct libevdev *evdev;	/**< The libevdev instance for this touch device. */

	struct input_absinfo abs_x;	/**< The abs info for X axis. */
	struct input_absinfo abs_y;	/**< The abs info for Y axis. */
	struct input_absinfo abs_mt_slot;	/**< The abs info for MT SLOT. */
	struct input_absinfo abs_mt_position_x;	/**< The abs info for MT POSITION X. */
	struct input_absinfo abs_mt_position_y;	/**< The abs info for MT POSITION Y. */
	struct input_absinfo abs_mt_tracking_id;	/**< The abs info for MT TRACKING ID. */

	device_input_touch_data_t touch_data;	/**< The touch data. */

	int is_primary_device;	/**< This device is primary device or not. */
};
typedef struct s_touch_device touch_device_t;

/**
 * Sub function for touch config setting.
 * It set some touch config to evdev instance.
 *
 * @param [in]	pdi	Pointer to struct s_device_input.
 * @return int
 * @retval	1	Has primary device.
 * @retval	0	Does not have primary device.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_has_primary_device(struct s_device_input *pdi)
{
	int result = 0;
	touch_device_t *ptouch_device = NULL;

	if (pdi == NULL) {
		result = -2;
		goto do_return;
	}

	dl_list_for_each(ptouch_device, &pdi->devices, touch_device_t, list) {
		if (ptouch_device->is_primary_device == 1) {
			result = 1;
			goto do_return;
		}
	}

do_return:
	return result;
}
/**
 * Sub function for touch config setting.
 * It set some touch config to evdev instance.
 *
 * @param [in]	pevdev	Pointer to evdev instance.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_uinput_set_touch_configs(struct libevdev *pevdev)
{
	int result = 0;

	if (pevdev == NULL) {
		result = -2;
		goto do_return;
	}

	// Set device name.
	libevdev_set_name(pevdev, g_touch_device_name);

	// Set device id.
	libevdev_set_id_bustype(pevdev, BUS_VIRTUAL);
	libevdev_set_id_version(pevdev, 1);

	// Enable Syn type
	libevdev_enable_event_type(pevdev, EV_SYN);

	// Enable Key type
	{
		libevdev_enable_event_type(pevdev, EV_KEY);
		libevdev_enable_event_code(pevdev, EV_KEY, BTN_TOUCH, NULL);
	}

	// Enable Abs type
	{
		struct input_absinfo absinfo = {
			.value = 0, .minimum = 0, .maximum = 0,
			.fuzz = 0, .flat = 0, .resolution = 0,
		};

		libevdev_enable_event_type(pevdev, EV_ABS);
		absinfo.maximum = g_touch_device_max_width;
		libevdev_enable_event_code(pevdev, EV_ABS, ABS_X, &absinfo);
		absinfo.maximum = g_touch_device_max_height;
		libevdev_enable_event_code(pevdev, EV_ABS, ABS_Y, &absinfo);
		absinfo.maximum = 64;
		libevdev_enable_event_code(pevdev, EV_ABS, ABS_MT_SLOT, &absinfo);
		absinfo.maximum = g_touch_device_max_width;
		libevdev_enable_event_code(pevdev, EV_ABS, ABS_MT_POSITION_X, &absinfo);
		absinfo.maximum = g_touch_device_max_height;
		libevdev_enable_event_code(pevdev, EV_ABS, ABS_MT_POSITION_Y, &absinfo);
		absinfo.maximum = 65535;
		libevdev_enable_event_code(pevdev, EV_ABS, ABS_MT_TRACKING_ID, &absinfo);
	}

	// Enable Msc type
	{
		libevdev_enable_event_type(pevdev, EV_MSC);
		libevdev_enable_event_code(pevdev, EV_MSC, MSC_TIMESTAMP, NULL);
	}

do_return:

	return result;
}
/**
 * Sub function for uinput device handling.
 * Create target uinput device.
 *
 * @param [in]	pdi	Pointer to callback function to use device event notification.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_uinput_device_create(struct s_device_input *pdi)
{
	int ret = -1, result = 0;
	struct libevdev *pevdev = NULL;
	struct libevdev_uinput *puidev = NULL;

	if (pdi == NULL) {
		result = -2;
		goto do_return;
	}

	pevdev = libevdev_new();
	if (pevdev == NULL) {
		result = -1;
		goto do_return;
	}

	ret = device_input_uinput_set_touch_configs(pevdev);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	ret = libevdev_uinput_create_from_device(pevdev, LIBEVDEV_UINPUT_OPEN_MANAGED,  &puidev);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	pdi->evdev = pevdev;
	pevdev = NULL;
	pdi->uinput = puidev;
	puidev = NULL;

do_return:
	if (puidev != NULL) {
		libevdev_uinput_destroy(puidev);
	}

	if (pevdev != NULL) {
		libevdev_free(pevdev);
	}

	return result;
}

/**
 * Sub function for touch event handling.
 * Check to device, it shall handle or not.
 *
 * @param [in]	devname	Device name to check.
 * @return int
 * @retval	1	This device is target device (It's listed in config file).
 * @retval	0	This device is not target device. (Own device is not target device.)
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_is_target_device_name(const char *devname)
{
	int result = 0;

	if (devname == NULL) {
		result = -2;
		goto do_return;
	}

	// Todo: check to device, it is listed  in config file or not
	if (strncmp(devname, g_touch_device_name, sizeof(g_touch_device_name)) == 0) {
		result = 0;
	} else {
		result = 1;
	}
do_return:
	return result;
}
/**
 * Sub function for touch event handling.
 * Check to device, it is touch device or not.
 *
 * @param [in]	pevdev	Pointer to instance for struct libevdev.
 * @return int
 * @retval	1	This device is touch device.
 * @retval	0	This device is not touch device.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_is_touch_device(struct libevdev *pevdev)
{
	int result = 0;

	if (pevdev == NULL) {
		result = -2;
		goto do_return;
	}

	if (libevdev_has_event_type(pevdev, EV_ABS) == 1 ) {
		if (libevdev_has_event_code(pevdev, EV_ABS, ABS_MT_SLOT) &&
			libevdev_has_event_code(pevdev, EV_ABS, ABS_MT_POSITION_X) &&
			libevdev_has_event_code(pevdev, EV_ABS, ABS_MT_POSITION_Y) &&
			libevdev_has_event_code(pevdev, EV_ABS, ABS_MT_TRACKING_ID)) {
			result = 1;
		} else if (libevdev_has_event_code(pevdev, EV_ABS, ABS_X) &&
			libevdev_has_event_code(pevdev, EV_ABS, ABS_Y)) {
			result = 1;
		} else {
			result = 0;
		}
	} else {
		result = 0;
	}

do_return:
	return result;
}
#ifdef _PRINTF_DEBUG_
static void debug_push_touch_event(device_input_touch_data_t *ditd)
{
	// check arguments
	if (ditd == NULL) {
		goto do_return;
	}

	fprintf(stdout,"\n");

	for (size_t slot = 0; slot < ditd->num_slots; slot++) {
		int x = -1, y= -1, tracking_id = -1;
		if (ditd->mt_elements[slot].slot.valid == 1) {
			ssize_t s = ditd->mt_elements[slot].slot.value;
			fprintf(stdout,"slot=%zu\n", s);
		} else {
			if (slot == 0) {
				if ((ditd->mt_elements[0].position_x.valid == 1) ||
					(ditd->mt_elements[0].position_y.valid == 1) ||
					(ditd->mt_elements[0].tracking_id.valid == 1)) {
					fprintf(stdout,"slot=%zu (no slot info)\n", slot);
				}
			}
		}
		if (ditd->mt_elements[slot].position_x.valid == 1) {
			x = ditd->mt_elements[slot].position_x.value;
			fprintf(stdout,"mt_x=%d\n", x);
		}
		if (ditd->mt_elements[slot].position_y.valid == 1) {
			y = ditd->mt_elements[slot].position_y.value;
			fprintf(stdout,"mt_y=%d\n", y);
		}
		if (ditd->mt_elements[slot].tracking_id.valid == 1) {
			tracking_id = ditd->mt_elements[slot].tracking_id.value;
			fprintf(stdout,"mt_tracking_id=%d\n", tracking_id);
		}
	}

	if (ditd->abs_x.valid == 1) {
		int abs_x = ditd->abs_x.value;
		fprintf(stdout,"abs_x=%d\n", abs_x);
	}
	if (ditd->abs_y.valid == 1) {
		int abs_y = ditd->abs_y.value;
		fprintf(stdout,"abs_y=%d\n", abs_y);
	}
	if (ditd->btn_touch.valid == 1) {
		int btn_touch = ditd->btn_touch.value;
		fprintf(stdout,"btn_touch=%d\n", btn_touch);
	}
	if (ditd->timestamp.valid == 1) {
		uint32_t timestamp = ditd->timestamp.value;
		fprintf(stdout,"timestamp=%u\n", timestamp);
	}

do_return:
	return;
}
#endif
/**
 * Sub function for touch event handling.
 * Push touch event from data to uinput touch device.
 *
 * @param [in]	di	Pointer to top data that includes uinput device structure.
 * @param [in]	ditd	Pointer to data source touch data structure.
 * @return int
 * @retval	0	Push operation is successful.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_push_touch_event(struct s_device_input *di, device_input_touch_data_t *ditd)
{
	int result = 0, ret = -1;

	// check arguments
	if (di == NULL || ditd == NULL) {
		goto do_return;
	}
	

	for (size_t slot = 0; slot < ditd->num_slots; slot++) {
		int x = -1, y= -1, tracking_id = -1;
		if (ditd->mt_elements[slot].slot.valid == 1) {
			ssize_t s = ditd->mt_elements[slot].slot.value;
			(void)libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_MT_SLOT, s);
		} else {
			if (slot == 0) {
				if ((ditd->mt_elements[0].position_x.valid == 1) ||
					(ditd->mt_elements[0].position_y.valid == 1) ||
					(ditd->mt_elements[0].tracking_id.valid == 1)) {
					ret = libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_MT_SLOT, 0);
				}
			}
		}
		if (ditd->mt_elements[slot].position_x.valid == 1) {
			x = ditd->mt_elements[slot].position_x.value;
			(void)libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_MT_POSITION_X, x);
		}
		if (ditd->mt_elements[slot].position_y.valid == 1) {
			y = ditd->mt_elements[slot].position_y.value;
			(void)libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_MT_POSITION_Y, y);
		}
		if (ditd->mt_elements[slot].tracking_id.valid == 1) {
			tracking_id = ditd->mt_elements[slot].tracking_id.value;
			(void)libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_MT_TRACKING_ID, tracking_id);
		}
	}

	if (ditd->abs_x.valid == 1) {
		int abs_x = ditd->abs_x.value;
		(void)libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_X, abs_x);
	}
	if (ditd->abs_y.valid == 1) {
		int abs_y = ditd->abs_y.value;
		(void)libevdev_uinput_write_event(di->uinput, EV_ABS, ABS_Y, abs_y);
	}
	if (ditd->btn_touch.valid == 1) {
		int btn_touch = ditd->btn_touch.value;
		(void)libevdev_uinput_write_event(di->uinput, EV_KEY, BTN_TOUCH, btn_touch);
	}
	if (ditd->timestamp.valid == 1) {
		uint32_t timestamp = ditd->timestamp.value;
		(void)libevdev_uinput_write_event(di->uinput, EV_MSC, MSC_TIMESTAMP, timestamp);
	}

	(void)libevdev_uinput_write_event(di->uinput, EV_SYN, SYN_REPORT, 0);

	#ifdef _PRINTF_DEBUG_
	debug_push_touch_event(ditd);
	#endif

do_return:
	return result;
}

/**
 * Sub function for touch event handling.
 * Store touch event data to touch data structure.
 *
 * @param [inout]	dest	Pointer to destination touch data structure.
 * @param [in]	slot	Current slot number.
 * @param [in]	ev		Pointer to input event structure.
 * @return size_t
 * @retval	Next slot number.
 */
static size_t device_input_store_touch_event(device_input_touch_data_t *dest, size_t slot, struct input_event *ev)
{
	size_t return_slot = slot;

	// check arguments
	if (dest == NULL || ev == NULL) {
		goto do_return;
	}

	// check slot range
	if (slot >= dest->num_slots) {
		goto do_return;
	}

	// store event
	if (ev->type == EV_ABS) {
		if (ev->code == ABS_MT_SLOT) {
			return_slot = (size_t) ev->value;
			if (return_slot < dest->num_slots) {
				dest->mt_elements[return_slot].slot.value = ev->value;
				dest->mt_elements[return_slot].slot.valid = 1;
			} else {
				;	//nop
			}
		} else if (ev->code == ABS_MT_POSITION_X) {
			dest->mt_elements[slot].position_x.value = ev->value;
			dest->mt_elements[slot].position_x.valid = 1;
		} else if (ev->code == ABS_MT_POSITION_Y) {
			dest->mt_elements[slot].position_y.value = ev->value;
			dest->mt_elements[slot].position_y.valid = 1;
		} else if (ev->code == ABS_MT_TRACKING_ID) {
			dest->mt_elements[slot].tracking_id.value = ev->value;
			dest->mt_elements[slot].tracking_id.valid = 1;
		} else if (ev->code == ABS_X) {
			dest->abs_x.value = ev->value;
			dest->abs_x.valid = 1;
		} else if (ev->code == ABS_Y) {
			dest->abs_y.value = ev->value;
			dest->abs_y.valid = 1;
		} else {
			;	//nop
		}
	}

do_return:
	return return_slot;
}

/**
 * Sub function for touch event handling.
 * Clear touch event data into touch data structure.
 *
 * @param [inout]	dest	Pointer to destination touch data structure.
 * @return void
 */
static void device_input_clear_touch_event(device_input_touch_data_t *dest)
{
	// check arguments
	if (dest == NULL) {
		return;
	}

	// clear event
	for (size_t i = 0; i < dest->num_slots; i++) {
		dest->mt_elements[i].slot.valid = 0;
		dest->mt_elements[i].position_x.valid = 0;
		dest->mt_elements[i].position_y.valid = 0;
		dest->mt_elements[i].tracking_id.valid = 0;
	}
	dest->abs_x.valid = 0;
	dest->abs_y.valid = 0;
	dest->btn_touch.valid = 0;
	dest->timestamp.valid = 0;
}

/**
 * Event handler for touch device.
 * This function analyze received data using libevdev.
 *
 * @param [in]	ptd		Pointer to target touch device structure.
 * @return int
 * @retval	0	Success to event handling.
 * @retval	-1	Internal error (Not use).
 */
static int device_input_do_touch_device(struct s_touch_device *ptd)
{
	int ret = 0;
	size_t current_slots = 0;
	struct input_event ev;

	do {
		ret = libevdev_next_event(ptd->evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (ret == LIBEVDEV_READ_STATUS_SUCCESS) {
			if (ev.type == EV_ABS) {
				current_slots = device_input_store_touch_event(&ptd->touch_data, current_slots, &ev);
			} else if (ev.type == EV_KEY) {
				if (ev.code == BTN_TOUCH) {
					ptd->touch_data.btn_touch.value = ev.value;
					ptd->touch_data.btn_touch.valid = 1;
				}
			} else if (ev.type == EV_MSC) {
				if (ev.code == MSC_TIMESTAMP) {
					ptd->touch_data.timestamp.value = (uint32_t) ev.value;
					ptd->touch_data.timestamp.valid = 1;
				}
			} else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
				// Process stored touch data
				if (ptd->is_primary_device == 1) {
					// Primary device, so push event to uinput device
					device_input_push_touch_event(ptd->pdi,&ptd->touch_data);
				}

				// Clear stored data
				device_input_clear_touch_event(&ptd->touch_data);
			}
		} else {
			// No more event
			break;
		}
	} while(1);

	return ret;
}
/**
 * Event handler for input device.
 * This function handled fd event and dispatch touch events.
 *
 * @param [in]	event		event source object.
 * @param [in]	fd			File descriptor for input device.
 * @param [in]	revents		Active event (epoll).
 * @param [in]	userdata	Pointer to struct s_touch_device.
 * @return int
 * @retval	0	Success to event handling.
 * @retval	-1	Internal error (Not use).
 */
static int touch_event_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	int ret = 0;
	struct s_touch_device *ptd = NULL;

	if (userdata == NULL) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
		return 0;
	}

	ptd = (struct s_touch_device*)userdata;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// Fail safe - disable udev event
		struct s_device_input *pdi = ptd->pdi;
		#ifdef _PRINTF_DEBUG_
		fprintf(stdout,"remove device %s\n", libevdev_get_name(ptd->evdev));
		#endif
		dl_list_del(&ptd->list);
		sd_event_source_disable_unref(event);
		libevdev_free(ptd->evdev);
		(void) free(ptd);
		ret = device_input_has_primary_device(pdi);
		if (ret == 0) {
			// No primary device, so set first device to primary device
			touch_device_t *first_device = NULL;
			first_device = dl_list_first(&pdi->devices, touch_device_t, list);
			if (first_device != NULL) {
				first_device->is_primary_device = 1;
			}
		}
	} else if ((revents & EPOLLIN) != 0) {
		// Receive
		(void)device_input_do_touch_device(ptd);
	} else {
		;	//nop
	}

	return ret;
}
/**
 * Sub function for touch event handling.
 * Setup for the touch event loop.
 *
 * @param [in]	pdi	Pointer to instance for struct device_input.
 * @param [in]	pevdev	Pointer to instance for struct libevdev.
 * @return int
 * @retval	1	This device is touch device.
 * @retval	0	This device is not touch device.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
static int device_input_add_touch_device(struct s_device_input *pdi, struct libevdev *pevdev)
{
	int ret = -1, result = 0;
	struct s_touch_device *ptd = NULL;
	sd_event_source *touch_event_source = NULL;

	if (pdi == NULL || pevdev == NULL) {
		result = -2;
		goto do_return;
	}

	ptd = (struct s_touch_device *)malloc(sizeof(struct s_touch_device));
	if (ptd == NULL) {
		result = -1;
		goto do_return;
	}
	memset(ptd, 0, sizeof(struct s_touch_device));

	ret = libevdev_grab(pevdev, LIBEVDEV_GRAB);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	ret = sd_event_add_io(pdi->event, &touch_event_source, libevdev_get_fd(pevdev), (EPOLLHUP | EPOLLERR | EPOLLIN), touch_event_handler, (void*)ptd);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}


	dl_list_init(&ptd->list);
	ptd->pdi = pdi;
	ptd->evdev = pevdev;
	ptd->input_event_source = touch_event_source;

	{
		// Get input device parameters, set it to touch device structure.
		const struct input_absinfo *absinfo = NULL;

		absinfo = libevdev_get_abs_info(pevdev, ABS_X);
		if (absinfo != NULL) {
			ptd->abs_x = *absinfo;
		}

		absinfo = libevdev_get_abs_info(pevdev, ABS_Y);
		if (absinfo != NULL) {
			ptd->abs_y = *absinfo;
		}

		absinfo = libevdev_get_abs_info(pevdev, ABS_MT_SLOT);
		if (absinfo != NULL) {
			ptd->abs_mt_slot = *absinfo;
		}

		absinfo = libevdev_get_abs_info(pevdev, ABS_MT_POSITION_X);
		if (absinfo != NULL) {
			ptd->abs_mt_position_x = *absinfo;
		}

		absinfo = libevdev_get_abs_info(pevdev, ABS_MT_POSITION_Y);
		if (absinfo != NULL) {
			ptd->abs_mt_position_y = *absinfo;
		}

		absinfo = libevdev_get_abs_info(pevdev, ABS_MT_TRACKING_ID);
		if (absinfo != NULL) {
			ptd->abs_mt_tracking_id = *absinfo;
		}
	}

	if (ptd->abs_mt_slot.maximum > 0) {
		// Allocate MT element array
		ptd->touch_data.num_slots = (size_t)(ptd->abs_mt_slot.maximum + 1);
		ptd->touch_data.mt_elements = (device_input_abs_mt_element_t*) malloc(sizeof(device_input_abs_mt_element_t) * ptd->touch_data.num_slots);
		if (ptd->touch_data.mt_elements == NULL) {
			result = -1;
			goto do_return;
		}
		memset(ptd->touch_data.mt_elements, 0, sizeof(device_input_abs_mt_element_t) * ptd->touch_data.num_slots);
	}

	ret = device_input_has_primary_device(pdi);
	if (ret == 1) {
		ptd->is_primary_device = 0;
	} else {
		ptd->is_primary_device = 1;
	}

	dl_list_add_tail(&pdi->devices, &ptd->list);
	ptd = NULL;

do_return:
	(void) free(ptd);

	return result;
}

/**
 * External interface for touch event handling.
 * Add new input device to touch event handling structure.
 *
 * @param [in]	devnode	Device node path for new device.
 * @param [in]	pdi	Pointer to callback function to use device event notification.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 * @retval	-3	This device not a touch device.
 */
int device_input_add_new_device(const char *devnode, device_input_t *pdi)
{
	int fd = -1, ret = -1, result = 0;
	const char *devname = NULL;
	struct libevdev *pevdev = NULL;
	struct s_device_input *di = NULL;

	if (devnode == NULL || pdi == NULL) {
		result = -2;
		goto do_return;
	}

	di = (struct s_device_input*)pdi;

	fd = open(devnode, O_RDONLY|O_NONBLOCK);
	if (fd < 0) {
		result = -1;
		goto do_return;
	}

	ret = libevdev_new_from_fd(fd, &pevdev);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	devname = libevdev_get_name(pevdev);
	ret = device_input_is_target_device_name(devname);
	if (ret == 1) {
		// This device is target device.
		ret = device_input_is_touch_device(pevdev);
		if (ret == 1) {
			// This device is touch device.
			ret = device_input_add_touch_device(di, pevdev);
			if (ret < 0) {
				result = -3;
				goto do_return;
			}
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"add new touch device %s (%s)\n", devnode, devname);
			#endif
			result = 0;
			pevdev = NULL;
			goto do_return;
		} else {
			// This device is not touch device.
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"add new non-touch device %s (%s)\n", devnode, devname);
			#endif
			result = -3;
			goto do_return;
		}
	} else {
		// This device is not target device.
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"add new unhandled device %s\n", devnode);
		#endif
		result = -3;
		goto do_return;
	}	

do_return:
	if (pevdev != NULL) {
		libevdev_free(pevdev);
	}

	return result;
}
/**
 * External interface for touch event handling.
 * Setup for the touch event loop.
 *
 * @param [inout]	ppdi	Double pointer to device_input_t.
 * @param [in]	pevent	Instance of sd_event. (main loop)
 * @return int
 * @retval	0	Success to setup event loop.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int device_input_setup(device_input_t **ppdi, sd_event *pevent)
{
	int ret = -1, result = 0;
	struct s_device_input *pdi = NULL;
	const char *devnode = NULL;

	if (ppdi == NULL || pevent == NULL) {
		result = -2;
		goto do_return;
	}	

	pdi = (struct s_device_input*) malloc(sizeof(struct s_device_input));
	if (pdi == NULL) {
		result = -1;
		goto do_return;
	}
	memset(pdi, 0, sizeof(struct s_device_input));

	ret = device_input_uinput_device_create(pdi);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	//devnode = libevdev_uinput_get_devnode(pdi->uinput);
	//(void)rename(devnode, "/dev/input/eventG0");	// Fixed device node for uinput device
	#ifdef _PRINTF_DEBUG_
	devnode = libevdev_uinput_get_devnode(pdi->uinput);
	(void) fprintf(stdout,"create uinput abs device %s\n", devnode);
	#endif

	pdi->event = pevent;
	dl_list_init(&pdi->devices);

	(*ppdi) = (device_input_t*) pdi;
	pdi = NULL;

do_return:
	(void) free(pdi);

	return result;
}
/**
 * External interface for touch event handling.
 * Cleanup for the touch event loop.
 *
 * @param [in]	pdi	Pointer to instance for struct device_input.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int device_input_cleanup(device_input_t *pdi)
{
	struct s_device_input *di = NULL;

	if (pdi == NULL) {
		return -2;
	}

	di = (struct s_device_input*)pdi;

	(void) free(di);

	return 0;
}
