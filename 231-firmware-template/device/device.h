#pragma once

#include <stdint.h>
#include "firmware.h"

#define DEVICE_NAME FIRMWARE_NAME
#define DEVICE_PROJECT FIRMWARE_PROJECT
#define DEVICE_REPO FIRMWARE_REPO

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif

typedef struct {
    const char *board;
    char serial[17];
    uint32_t manufacturer;
    uint32_t part;
    uint32_t revision;
    const char *sdk_version;
} device_info_t;

void device_get_info(device_info_t *info);
