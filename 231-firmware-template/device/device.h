#pragma once

#include "firmware.h"

#define DEVICE_NAME FIRMWARE_NAME
#define DEVICE_PROJECT FIRMWARE_PROJECT
#define DEVICE_REPO FIRMWARE_REPO

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif

void device_info(void);
