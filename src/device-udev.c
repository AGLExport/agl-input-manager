/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-udev.c
 * @brief	This file include device management functions using libudev.
 */
#include "device-udev.h"

#include <stdlib.h>
#include <systemd/sd-event.h>
#include <libudev.h>

#include <stdio.h>
#include <string.h>
#include <sys/sysmacros.h>

/**
 * @struct	s_dynamic_device_udev_listener
 * @brief	The listener information for device udev.
 */
struct s_dynamic_device_udev_listener {
	device_udev_listener_handler_t func;		/**< The pointer to handler. */
	void *userdata;								/**< The pointer to user data.  */
};

/**
 * @struct	s_dynamic_device_udev
 * @brief	The data structure for device monitor using libudev.
 */
struct s_dynamic_device_udev {
	struct udev* pudev;					/**< The udev object created by libudev. */
	struct udev_monitor *pudev_monitor;	/**< The udev_monitor object created by libudev.  */
	sd_event_source *libudev_source ;	/**< The sd event source controlled by libudev. */

	struct s_dynamic_device_udev_listener listener;	/** The listener information. */
};

static int device_control_dynamic_udev_devevent(dynamic_device_udev_t *pddu);
static int device_control_dynamic_udev_create_info(device_udev_info_t *udi, struct udev_list_entry *le);

/**
 * Event handler for libudev.
 * This function analyze received data using udev_monitor by libudev.
 *
 * @param [in]	event		libudev event source object.
 * @param [in]	fd			File descriptor for udev_monitor.
 * @param [in]	revents		Active event (epoll).
 * @param [in]	userdata	Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to event handling.
 * @retval	-1	Internal error (Not use).
 */
static int udev_event_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	int ret = 0;
	dynamic_device_udev_t *pddu = NULL;

	if (userdata == NULL) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
		return 0;
	}

	pddu = (dynamic_device_udev_t*)userdata;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
	} else if ((revents & EPOLLIN) != 0) {
		// Receive
		(void)device_control_dynamic_udev_devevent(pddu);
	} else {
		;	//nop
	}

	return ret;
}

/**
 * Sub function for uevent monitor.
 * This function exec listener function.
 *
 * @param [in]	ddu		Pointer to struct s_dynamic_device_udev.
 * @param [in]	pudi	Pointer to device_udev_info_t.
 * @return int
 * @retval	0	Success to get device info.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error. (Reserve)
 */
static int device_control_dynamic_exec_listener(struct s_dynamic_device_udev *ddu, device_udev_info_t *pudi)
{
	int result = 0;

	if ((pudi->dev_node == NULL) || (pudi->action == NULL)) {
		// Do not exec listener
		result = -2;
		goto do_return;
	}

	if (ddu->listener.func == NULL) {
		// Do not exec listener
		result = -2;
		goto do_return;
	}

	(void) ddu->listener.func(pudi, ddu->listener.userdata);

do_return:

	return result;
}

/**
 * Sub function for uevent monitor.
 * This function analyze uevent and injection to guest if necessary.
 *
 * @param [in]	pddu	Pointer to dynamic_device_udev_t.
 * @return int
 * @retval	0	Success to get device info.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error. (Reserve)
 */
static int device_control_dynamic_udev_devevent(dynamic_device_udev_t *pddu)
{
	int ret = -1, result = 0;
	struct s_dynamic_device_udev *ddu = NULL;
	struct udev_device *pdev = NULL;
	struct udev_list_entry *le = NULL;
	device_udev_info_t udi;

	if (pddu == NULL) {
		result = -2;
		goto do_return;
	}
	ddu = (struct s_dynamic_device_udev*)pddu;
	pdev = udev_monitor_receive_device(ddu->pudev_monitor);
	if (pdev == NULL) {
		result = -1;
		goto do_return;
	}

	le = udev_device_get_properties_list_entry(pdev);
	if (le == NULL) {
		goto do_return;	// No data.
	}

	(void) memset(&udi, 0, sizeof(udi));
	ret = device_control_dynamic_udev_create_info(&udi, le);
	if (ret == 0) {
		(void) device_control_dynamic_exec_listener(ddu, &udi);
	}

do_return:
	if (pdev != NULL) {
		(void) udev_device_unref(pdev);
	}

	return result;
}

/**
 * Sub function for uevent monitor.
 * This function create uevent info to use device assignment check and operations from udev properties list.
 *
 * @param [out]	udi		Pointer to uevent_device_info_t.
 * @param [in]	le		Pointer to udev_list_entry.
 * @return int
 * @retval	0	Success to get device info.
 * @retval	-1	Internal error.
 * @retval	-2	Argument error. (Reserve)
 */
static int device_control_dynamic_udev_create_info(device_udev_info_t *udi, struct udev_list_entry *le)
{
	while (le != NULL) {
		const char* elem_name = NULL;
		const char* elem_value = NULL;

		elem_name = udev_list_entry_get_name(le);
		elem_value = udev_list_entry_get_value(le);

		if (strcmp(elem_name, "ACTION") == 0) {
			// set to uevent device info
			udi->action = elem_value;

		} else if (strcmp(elem_name, "DEVNAME") == 0) {
			// set to uevent device info
			udi->dev_node = elem_value;

		} else if (strcmp(elem_name, "SUBSYSTEM") == 0) {
			// set to uevent device info
			udi->sub_system = elem_value;

		} else {
			;	//skip this data
		}

		le = udev_list_entry_get_next(le);
	}

	return 0;
}

/**
 * External interface function for uevent monitor.
 * Scan device operation.
 *
 * @param [in]	pddu		Pointer to dynamic_device_udev_t.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int device_udev_scan(dynamic_device_udev_t *pddu)
{
	int ret = -1;
	int result = 0;
	struct s_dynamic_device_udev *ddu = NULL;
	struct udev_enumerate *udev_enum = NULL;
	struct udev_list_entry *devices = NULL, *dev_list_entry = NULL;

	if (pddu == NULL) {
		result = -2;
		goto do_return;
	}

	ddu = (struct s_dynamic_device_udev*)pddu;

	if (ddu->pudev == NULL) {
		result = -2;
		goto do_return;
	}

	udev_enum = udev_enumerate_new(ddu->pudev);
	if (udev_enum == NULL) {
		result = -1;
		goto do_return;
	}

	ret = udev_enumerate_add_match_subsystem(udev_enum, "input");
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	ret = udev_enumerate_scan_devices(udev_enum);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	devices = udev_enumerate_get_list_entry(udev_enum);
	if (devices == NULL) {
		result = -1;
		goto do_return;
	}

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path = NULL, *subsys = NULL, *devnode = NULL;
		struct udev_device *pdev = NULL;
		const char *action_str = "add";
		device_udev_info_t udi;

		(void) memset(&udi, 0, sizeof(udi));

		path = udev_list_entry_get_name(dev_list_entry);
		if (path == NULL)
			continue;

		pdev = udev_device_new_from_syspath(ddu->pudev, path);
		if (pdev == NULL)
			continue;

		devnode = udev_device_get_devnode(pdev);
		if (devnode == NULL) {
			udev_device_unref(pdev);
			continue;
		}

		subsys = udev_device_get_subsystem(pdev);
		if (subsys == NULL) {
			udev_device_unref(pdev);
			continue;
		}

		udi.action = action_str;
		udi.dev_node = devnode;
		udi.sub_system = subsys;

		ret = device_control_dynamic_exec_listener(ddu, &udi);
		if(ret < 0) {
			; // no operation
		}

		// free device
		udev_device_unref(pdev);
	}

do_return:
	if (udev_enum != NULL) {
		udev_enumerate_unref(udev_enum);
	}

	return result;
}
/**
 * Sub function for uevent monitor.
 * Setup for the uevent monitor event loop.
 *
 * @param [inout]	ppddu	Double pointer to dynamic_device_udev_t.
 * @param [in]	pevent	Instance of sd_event. (main loop)
 * @param [in]	plistener	Pointer to callback function to use device event notification.
 * @param [in]	puserdata	Pointer to userdata that will set to argument for callback function.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int device_udev_setup(dynamic_device_udev_t **ppddu, sd_event *pevent, device_udev_listener_handler_t plistener, void *puserdata)
{
	struct s_dynamic_device_udev *ddu = NULL;
	struct udev* pudev = NULL;
	struct udev_monitor *pudev_monitor = NULL;
	sd_event_source *libudev_source = NULL;
	int fd = -1;
	int ret = -1;
	int result = -1;

	if ((ppddu == NULL) || (pevent == NULL) || (plistener == NULL)) {
		result = -2;
		goto err_return;
	}

	ddu = malloc(sizeof(struct s_dynamic_device_udev));
	if (ddu == NULL) {
		result = -1;
		goto err_return;
	}

	(void) memset(ddu, 0, sizeof(struct s_dynamic_device_udev));

	pudev = udev_new();
	if (pudev == NULL) {
		result = -1;
		goto err_return;
	}

	pudev_monitor = udev_monitor_new_from_netlink(pudev, "udev");
	if (pudev_monitor == NULL) {
		result = -1;
		goto err_return;
	}

	ret =  udev_monitor_filter_add_match_subsystem_devtype(pudev_monitor, "input", NULL);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	ret = udev_monitor_enable_receiving(pudev_monitor);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	fd = udev_monitor_get_fd(pudev_monitor);
	if (fd < 0) {
		result = -1;
		goto err_return;
	}

	ret = sd_event_add_io(pevent, &libudev_source, fd, EPOLLIN, udev_event_handler, ddu);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	ddu->pudev = pudev;
	ddu->pudev_monitor = pudev_monitor;
	ddu->libudev_source = libudev_source;
	ddu->listener.func = plistener;
	ddu->listener.userdata = puserdata;

	(*ppddu) = (dynamic_device_udev_t*)ddu;

	return 0;

err_return:
	if (libudev_source != NULL) {
		(void) sd_event_source_disable_unref(libudev_source);
	}

	if (pudev_monitor != NULL) {
		(void) udev_monitor_unref(pudev_monitor);
	}

	if (pudev != NULL) {
		(void) udev_unref(pudev);
	}

	if (ddu != NULL) {
		(void) free(ddu);
	}

	(*ppddu) = NULL;

	return result;
}
/**
 * Sub function for uevent monitor.
 * Cleanup for the uevent monitor event loop.
 *
 * @param [in]	ddm		Pointer to dynamic_device_manager_t.
 * @return int
 * @retval	0	Success to change device infomation at list.
 * @retval	-1	Internal error. (Reserve)
 * @retval	-2	Argument error.
 */
int device_udev_cleanup(dynamic_device_udev_t *pddu)
{
	struct s_dynamic_device_udev *ddu = NULL;

	if (pddu == NULL) {
		return -2;
	}

	ddu = (struct s_dynamic_device_udev*)pddu;

	if (ddu->libudev_source != NULL) {
		(void) sd_event_source_disable_unref(ddu->libudev_source);
	}

	if (ddu->pudev_monitor != NULL) {
		(void) udev_monitor_unref(ddu->pudev_monitor);
	}

	if (ddu->pudev != NULL) {
		(void) udev_unref(ddu->pudev);
	}

	(void) free(ddu);

	return 0;
}
