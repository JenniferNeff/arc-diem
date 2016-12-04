/**
 * This file was generated with Enamel : http://gregoiresage.github.io/enamel
 */

#ifndef ENAMEL_H
#define ENAMEL_H

#include <pebble.h>

// -----------------------------------------------------
// Getter for 'DayStart'
#define DAYSTART_PRECISION 1
int32_t enamel_get_DayStart();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'DayEnd'
#define DAYEND_PRECISION 1
int32_t enamel_get_DayEnd();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'BatteryStatus'
const char* enamel_get_BatteryStatus();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'BluetoothStatus'
const char* enamel_get_BluetoothStatus();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'BluetoothDisconnect'
const char* enamel_get_BluetoothDisconnect();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'BluetoothConnect'
const char* enamel_get_BluetoothConnect();
// -----------------------------------------------------

void enamel_init();

void enamel_deinit();

typedef void* EventHandle;
typedef void(EnamelSettingsReceivedHandler)(void* context);

EventHandle enamel_settings_received_subscribe(EnamelSettingsReceivedHandler *handler, void *context);
void enamel_settings_received_unsubscribe(EventHandle handle);

#endif