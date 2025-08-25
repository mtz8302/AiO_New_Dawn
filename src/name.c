// To give your project a unique name, this code must be
// placed into a .c file (its own tab).  It can not be in
// a .cpp file or your main sketch (the .ino file).

#include <usb_names.h>

// Edit these lines to create your own name.  The length must
// match the number of characters in your custom name.

#define PRODUCT_NAME   {'A','i','O',' ','v','6','-','N','G'}    // Bus reported device description
#define PRODUCT_NAME_LEN  9

#define MANUFACTURER_NAME  {'A','g','O','p','e','n','G','P','S'}
#define MANUFACTURER_NAME_LEN 9

#define SERIAL_NUMBER_NAME {' ','1','.','0','.','4'}
#define SERIAL_NUMBER_LEN  6

// First USB Serial port strings (Console)
#define CDC_STATUS_NAME   {'C','o','n','s','o','l','e'}
#define CDC_STATUS_NAME_LEN  7

// Second USB Serial port strings (GPS1)
#define CDC2_STATUS_NAME   {'G','P','S','1','_','R','i','g','h','t'}
#define CDC2_STATUS_NAME_LEN  10

// Third USB Serial port strings (GPS2)
#define CDC3_STATUS_NAME   {'G','P','S','2','_','L','e','f','t'}
#define CDC3_STATUS_NAME_LEN  9

// Do not change this part.  This exact format is required by USB.

struct usb_string_descriptor_struct usb_string_product_name = {
        2 + PRODUCT_NAME_LEN * 2,
        3,
        PRODUCT_NAME
};

struct usb_string_descriptor_struct usb_string_serial_number = {
        2 + SERIAL_NUMBER_LEN * 2,
        3,
        SERIAL_NUMBER_NAME
};

struct usb_string_descriptor_struct usb_string_manufacturer_name = {
        2 + MANUFACTURER_NAME_LEN * 2,
        3,
        MANUFACTURER_NAME
};

// CDC interface strings
struct usb_string_descriptor_struct usb_string_cdc_status_interface = {
        2 + CDC_STATUS_NAME_LEN * 2,
        3,
        CDC_STATUS_NAME
};

struct usb_string_descriptor_struct usb_string_cdc_data_interface = {
        2 + CDC_STATUS_NAME_LEN * 2,
        3,
        CDC_STATUS_NAME
};

struct usb_string_descriptor_struct usb_string_cdc2_status_interface = {
        2 + CDC2_STATUS_NAME_LEN * 2,
        3,
        CDC2_STATUS_NAME
};

struct usb_string_descriptor_struct usb_string_cdc2_data_interface = {
        2 + CDC2_STATUS_NAME_LEN * 2,
        3,
        CDC2_STATUS_NAME
};

struct usb_string_descriptor_struct usb_string_cdc3_status_interface = {
        2 + CDC3_STATUS_NAME_LEN * 2,
        3,
        CDC3_STATUS_NAME
};

struct usb_string_descriptor_struct usb_string_cdc3_data_interface = {
        2 + CDC3_STATUS_NAME_LEN * 2,
        3,
        CDC3_STATUS_NAME
};