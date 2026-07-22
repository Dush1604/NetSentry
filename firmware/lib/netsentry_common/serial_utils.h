
#ifndef SERIAL_UTILS_H
#define SERIAL_UTILS_H
#include <Arduino.h>

extern SemaphoreHandle_t networkMutex;
void initNetworkMutex();
void initSerialMutex();
void safePrintln(const String& msg);

#endif
