#pragma once

#include <stdbool.h>

bool app_init(void);
void TaskServo(void *pvParameters);
void TaskOLED(void *pvParameters);
void TaskPot(void *pvParameters);
void TaskWiFi(void *pvParameters);



