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
#include "device-input.h"
#include "signal-util.h"

/**
 * SIGTERM handler to use receive shutdown request from init.
 *
 * @param [in]	si			Detail of received signal. Refer to Linux MAN.
 * @param [in]	userdata	Pointer to container_control_interface_t.
 * @return int
 * @retval 0	Success to setup container manager external interface.
 * @retval -1	Internal error. (Force event loop exit.)
 */
static int sigterm_notify(const struct signalfd_siginfo *si, void *userdata)
{
	sd_event *event = (sd_event *)userdata;

	(void) sd_event_exit(event, 0);

	return 0;
}
/**
 * @var		util_array
 * @brief	Signal handling information to use signal util.
 */
static signal_util_t util_array[1] = {
	[0] = {
		.signal = SIGTERM,
		.userdata = NULL,
		.signal_notify = sigterm_notify
	}
};

static int device_udev_listener_handler(device_udev_info_t *pdui, void* puserdata)
{
	device_input_t *pdi = (device_input_t *)puserdata;

	#ifdef _PRINTF_DEBUG_
	//(void) fprintf(stdout,"udi: action=%s devnode=%s subsystem=%s\n", pdui->action, pdui->dev_node, pdui->sub_system);
	#endif
	device_input_add_new_device(pdui->dev_node, pdi);

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
	device_input_t *ppdi = NULL;

	ret = sd_event_default(&event);
	if (ret < 0) {
		result = -1;
		goto finish;
	}

	util_array[0].userdata = (void*)event;
	ret = signal_setup(event, util_array, 1);
	if (ret < 0) {
		result = -1;
		goto finish;
	}

	ret = device_input_setup(&ppdi, event);
	if (ret < 0) {
		result = -1;
		goto finish;
	}

	ret = device_udev_setup(&pddu, event, device_udev_listener_handler, (void*)ppdi);
	if (ret < 0) {
		result = -1;
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

	device_udev_scan(pddu);

	ret = sd_event_loop(event);
	if (ret < 0) {
		result = ret;
	}

finish:
	(void) device_input_cleanup(ppdi);
	(void) device_udev_cleanup(pddu);
	(void) sd_event_unref(event);

	return result;
}
