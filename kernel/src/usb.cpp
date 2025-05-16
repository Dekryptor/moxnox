module;

#include <stdint.h>
#include <stddef.h>

export module usb;

export enum class usb_speed_class {
    unknown,
    low_speed,
    full_speed,
    high_speed,
    super_speed,
    super_speed_plus
};

export enum class usb_class_code : uint8_t {
    per_interface = 0,
    audio = 1,
    communications = 2,
    hid = 3,
    physical = 5,
    image = 6,
    printer = 7,
    mass_storage = 8,
    hub = 9,
    cdc_data = 10,
    smart_card = 11,
    content_security = 13,
    video = 14,
    personal_healthcare = 15,
    audio_video_devices = 16,
    billboard_device = 17,
    type_c_bridge = 18,
    bulk_display_protocol_device = 19,
    mctp_over_usb_endpoint_device = 20,
    i3c_device = 60,
    diagonstic_device = 220,
    wireless_controller = 224,
    miscellaneous = 239,
    application_specific = 254,
    vendor_specific = 255
};

export const uint8_t USB_REQUEST_TYPE_HOST_TO_DEVICE = 0x00;
export const uint8_t USB_REQUEST_TYPE_DEVICE_TO_HOST = 0x80;
export const uint8_t USB_REQUEST_TYPE_STANDARD = 0x00;
export const uint8_t USB_REQUEST_TYPE_CLASS = 0x20;
export const uint8_t USB_REQUEST_TYPE_VENDOR = 0x40;
export const uint8_t USB_REQUEST_TYPE_RECIPIENT_DEVICE = 0x00;
export const uint8_t USB_REQUEST_TYPE_RECIPIENT_INTERFACE = 0x01;
export const uint8_t USB_REQUEST_TYPE_RECIPIENT_ENDPOINT = 0x02;
export const uint8_t USB_REQUEST_TYPE_RECIPIENT_OTHER = 0x03;

export enum class usb_request : uint8_t {
    GET_STATUS = 0,
    CLEAR_FEATURE = 1,
    SET_FEATURE = 3,
    SET_ADDRESS = 5,
    GET_DESCRIPTOR = 6,
    SET_DESCRIPTOR = 7,
    GET_CONFIGURATION = 8,
    SET_CONFIGURATION = 9,
    GET_INTERFACE = 10,
    SET_INTERFACE = 11,
    SYNCH_FRAME = 12,
    SET_ENCRYPTION = 13,
    GET_ENCRYPTION = 14,
    SET_HANDSHAKE = 15,
    GET_HANDSHAKE = 16,
    SET_CONNECTION = 17,
    SET_SECURITY_DATA = 18,
    GET_SECURITY_DATA = 19,
    SET_WUSB_DATA = 20,
    LOOPBACK_DATA_WRITE = 21,
    LOOPBACK_DATA_READ = 22,
    SET_INTERFACE_DS = 23,
    GET_FW_STATUS = 26,
    SET_FW_STATUS = 27,
    SET_SEL = 48,
    SET_ISOCH_DELAY = 49,
};

export enum class usb_descriptor {
    DEVICE = 1,
    CONFIGURATION = 2,
    STRING = 3,
    INTERFACE = 4,
    ENDPOINT = 5,
    INTERFACE_POWER = 8,
    OTG = 9,
    _DEBUG = 10,
    INTERFACE_ASSOCIATION = 11,
    BOS = 15,
    DEVICE_CAPABILITY = 16,
    HID = 33,
    HUB = 41,
    SUPERSPEED_USB_ENDPOINT_COMPANION = 48,
    SUPERSPEEDPLUS_ISOCHRONOUS_ENDPOINT_COMPANION = 49,
};

export struct usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    enum usb_class_code bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute((packed));

static_assert(sizeof(usb_device_descriptor) == 0x12);

export struct usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    enum usb_class_code bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute((packed));

static_assert(sizeof(usb_interface_descriptor) == 0x9);

export struct usb_configuration_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute((packed));

static_assert(sizeof(usb_configuration_descriptor) == 0x9);

export struct usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t endpoint_number : 4;
    uint8_t reserved1 : 3;
    uint8_t direction : 1;
    uint8_t transfer_type : 2;
    uint8_t synchronization_type : 2;
    uint8_t usage_type : 2;
    uint8_t reserved2 : 2;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute((packed));

static_assert(sizeof(usb_endpoint_descriptor) == 0x7);

export const uint8_t USB_ENDPOINT_DIRECTION_OUT = 0x00;
export const uint8_t USB_ENDPOINT_DIRECTION_IN = 0x01;

export const uint8_t USB_ENDPOINT_TRANSFER_TYPE_CONTROL = 0x00;
export const uint8_t USB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS = 0x01;
export const uint8_t USB_ENDPOINT_TRANSFER_TYPE_BULK = 0x02;
export const uint8_t USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT = 0x03;

export const uint8_t USB_ENDPOINT_USAGE_TYPE_PERIODIC = 0x00;
export const uint8_t USB_ENDPOINT_USAGE_TYPE_NOTIFICATION = 0x01;

export struct usb_hid_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bDescriptorType2;
    uint16_t wDescriptorLength;
} __attribute((packed));

static_assert(sizeof(usb_hid_descriptor) == 0x9);

export struct usb2_hub_descriptor {
    uint8_t bDescLength;
    uint8_t bDescriptorType;
    uint8_t bNbrPorts;
    uint8_t power_switching_mode : 2;
    uint8_t compound_device : 1;
    uint8_t overcurrent_protection_mode : 2;
    uint8_t tt_think_time : 2;
    uint8_t port_indicators_supported : 1;
    uint8_t reserved;
    uint8_t bPwrOn2PwrGood;
    uint8_t bHubContrCurrent;
    uint8_t DeviceRemovable[0];
} __attribute((packed));

static_assert(offsetof(usb2_hub_descriptor, DeviceRemovable) == 0x7);

export struct usb_interface_association_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} __attribute((packed));

static_assert(sizeof(usb_interface_association_descriptor) == 0x8);

export enum usb2_port_feature_selector : uint8_t {
    PORT_CONNECTION = 0,
    PORT_ENABLE = 1,
    PORT_SUSPEND = 2,
    PORT_OVER_CURRENT = 3,
    PORT_RESET = 4,
    PORT_POWER = 8,
    PORT_LOW_SPEED = 9,
    C_PORT_CONNECTION = 16,
    C_PORT_ENABLE = 17,
    C_PORT_SUSPEND = 18,
    C_PORT_OVER_CURRENT = 19,
    C_PORT_RESET = 20,
    PORT_TEST = 21,
    PORT_INDICATOR = 22,
};
