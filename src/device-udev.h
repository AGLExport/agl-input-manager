/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-udev.h
 * @brief	The header for device management functions using libudev.
 */
#ifndef DEVICE_UDEV_H
#define DEVICE_UDEV_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>

/**
 * @struct	s_device_udev_info
 * @brief	The data structure for device assignment rule check.
 */
struct s_device_udev_info {
	const char *dev_node;	/**< The device node name. */
	const char *sub_system;	/**< The subsystem property from libudev. */
	const char *action;		/**< The action property from libudev. */
};
typedef struct s_device_udev_info device_udev_info_t;	/**< typedef for struct s_device_udev_info. */

typedef void* dynamic_device_udev_t;
typedef int (*device_udev_listener_handler_t)(device_udev_info_t *pdui, void* puserdata);
//-----------------------------------------------------------------------------
int device_udev_setup(dynamic_device_udev_t **ppddu, sd_event *pevent, device_udev_listener_handler_t plistener, void *puserdata);
int device_udev_scan(dynamic_device_udev_t *pddu);
int device_udev_cleanup(dynamic_device_udev_t *pddu);

//-----------------------------------------------------------------------------
#endif //#ifndef DEVICE_UDEV_H
