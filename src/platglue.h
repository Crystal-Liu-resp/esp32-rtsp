#pragma once

#include "sdkconfig.h"
#if (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)
#include "port-esp32.h"
#else
#include "port-posix.h"
#endif
