#pragma once

#if !__linux
#include "port-esp32.h"
#include "esp_log.h"
#else
#include "port-posix.h"
#endif
