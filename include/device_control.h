#pragma once

#ifndef TOUCH_DEBUG_ENABLED
#define TOUCH_DEBUG_ENABLED 0
#endif

void factoryReset();
void goToSleep();
void initializeTouchCalibration();
void taskControl(void* parameter);
#if TOUCH_DEBUG_ENABLED
void updateTouchTest(unsigned long now);
#endif
