/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	device-udev.h
 * @brief	The header for device management functions using libudev.
 */
#ifndef DEVICE_INPUT_H
#define DEVICE_INPUT_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>


typedef void* device_input_t;
//-----------------------------------------------------------------------------
int device_input_setup(device_input_t **ppdi, sd_event *pevent);
int device_input_cleanup(device_input_t *pdi);
int device_input_add_new_device(const char *devnode, device_input_t *pdi);

//-----------------------------------------------------------------------------
#endif //#ifndef DEVICE_INPUT_H
