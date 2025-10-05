/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	agl-input-manager.c
 * @brief	main source file for input management daemon for container host.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>

#include "device-udev.h"

static int device_udev_listener_handler(device_udev_info_t *pdui, void* puserdata)
{
	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"udi: action=%s devnode=%s subsystem=%s\n", pdui->action, pdui->dev_node, pdui->sub_system);
	#endif

	return 0;
}

/**
 * The main function for agl-input-manager.
 */
int main(int argc, char *argv[])
{
	int ret = -1, result = 0;
	sd_event *event = NULL;
	dynamic_device_udev_t *pddu = NULL;

	ret = sd_event_default(&event);
	if (ret < 0) {
		goto finish;
	}

	/*
	util_array[0].userdata = (void*)cci;
	ret = signal_setup(event, util_array, 1);
	if (ret < 0) {
		result = -1;
		goto finish;
	}
	*/

	ret = device_udev_setup(&pddu, event, device_udev_listener_handler, NULL);
	if (ret < 0) {
		goto finish;
	}

	// Enable automatic service watchdog support
	ret = sd_event_set_watchdog(event, 1);
	if (ret < 0) {
		result = -1;
		goto finish;
	}

	(void) sd_notify(
		1,
		"READY=1\n"
		"STATUS=Daemon startup completed, processing events.");

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"agl-input-manager: do device scan\n");
	#endif
	device_udev_scan(pddu);

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"agl-input-manager: start event loop\n");
	#endif
	ret = sd_event_loop(event);
	if (ret < 0) {
		result = ret;
	}

finish:
	event = sd_event_unref(event);

	return result;
}
