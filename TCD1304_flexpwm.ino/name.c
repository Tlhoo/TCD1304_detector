//This file is used to set a unique name/identifier to the Teensy.
//The unique identifier later allows for Matlab to easily find the
//Teensy in a list of serial devices, that would otherwise be named 
//as list of weird numbers, that may even change.
// To give your project a unique name, this code must be
// placed into a .c file (its own tab).  It can not be in
// a .cpp file or your main sketch (the .ino file).

#include "usb_names.h"

// Edit these lines to create your own name.  The length must
// match the number of characters in your custom name.

#define MANUFACTURER_NAME   {'T','e','e','n','s','y','4','.','0'}
#define MANUFACTURER_NAME_LEN  9
#define PRODUCT_NAME   {'D','E','T','E','C','T','O','R','1'}
#define PRODUCT_NAME_LEN  9
#define SERIAL_NUMBER {'M','C','1','2','3','4','5','6','9'}
#define SERIAL_NUMBER_LEN 9


// Do not change this part.  This exact format is required by USB.

struct usb_string_descriptor_struct usb_string_manufacturer_name = {
  2 + MANUFACTURER_NAME_LEN * 2,
  3,
  MANUFACTURER_NAME};

struct usb_string_descriptor_struct usb_string_product_name = {
        2 + PRODUCT_NAME_LEN * 2,
        3,
        PRODUCT_NAME
};

struct usb_string_descriptor_struct usb_string_serial_number = {
  2 + SERIAL_NUMBER_LEN * 2,
  3,
  SERIAL_NUMBER};
