#ifndef BT_DIAGNOSTICS_H
#define BT_DIAGNOSTICS_H

#include <Arduino.h>

/**
 * Run comprehensive Bluetooth diagnostics
 * Checks hardware status, memory, power, pin conflicts, and file system
 * Prints detailed output to Serial for debugging
 */
void runBluetoothDiagnostics();

#endif
