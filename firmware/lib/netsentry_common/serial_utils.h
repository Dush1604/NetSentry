
#ifndef SERIAL_UTILS_H
#define SERIAL_UTILS_H
#include <Arduino.h>

void initSerialMutex();
void safePrintln(const String& msg);

#endif
