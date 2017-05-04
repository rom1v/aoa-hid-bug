#include <errno.h>
#include <error.h>
#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// HID report descriptor recorded from a random wired mouse
static const unsigned char REPORT_DESC[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01,
    0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01,
    0x81, 0x02, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08,
    0x95, 0x03, 0x31, 0x06, 0xC0, 0xC0
};
#define REPORT_DESC_SIZE (sizeof(REPORT_DESC)/sizeof(REPORT_DESC[0]))

// some HID mouse event recorded from the mouse
static const unsigned char HID_EVENT[] = {
    0x00, 0x02, 0xf8, 0x00
};
#define HID_EVENT_SIZE (sizeof(HID_EVENT)/sizeof(HID_EVENT[0]))

// <https://source.android.com/devices/accessories/aoa2#hid-support>
#define AOA_REGISTER_HID        54
#define AOA_UNREGISTER_HID      55
#define AOA_SET_HID_REPORT_DESC 56
#define AOA_SEND_HID_EVENT      57

#define DEFAULT_TIMEOUT 1000

void print_libusb_error(enum libusb_error errcode) {
    fprintf(stderr, "%s\n", libusb_strerror(errcode));
}

libusb_device *find_device(uint16_t vid, uint16_t pid) {
    libusb_device **list;
    libusb_device *found = NULL;
    ssize_t cnt = libusb_get_device_list(NULL, &list);
    ssize_t i = 0;
    if (cnt < 0) {
        print_libusb_error(cnt);
        return NULL;
    }
    for (i = 0; i < cnt; ++i) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(device, &desc);
        if (vid == desc.idVendor && pid == desc.idProduct) {
            libusb_ref_device(device);
            found = device;
            break;
        }
    }
    libusb_free_device_list(list, 1);
    return found;
}

int register_hid(libusb_device_handle *handle, uint16_t descriptor_size) {
    const uint8_t requestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    const uint8_t request = AOA_REGISTER_HID;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): total length of the HID report descriptor
    const uint16_t value = 0;
    const uint16_t index = descriptor_size;
    unsigned char *const buffer = NULL;
    const uint16_t length = 0;
    const unsigned int timeout = DEFAULT_TIMEOUT;
    int r = libusb_control_transfer(handle, requestType, request,
                                    value, index, buffer, length, timeout);
    if (r < 0) {
        print_libusb_error(r);
        return 1;
    }
    return 0;
}

int send_hid_descriptor(libusb_device_handle *handle,
                        const unsigned char *descriptor, uint16_t size,
                        uint8_t max_packet_size_0) {
    const uint8_t requestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    const uint8_t request = AOA_SET_HID_REPORT_DESC;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    const uint16_t value = 0;
    // libusb_control_transfer expects non-const but should not modify it
    unsigned char *const buffer = (unsigned char *) descriptor;
    const unsigned int timeout = DEFAULT_TIMEOUT;
    /*
     * If the HID descriptor is longer than the endpoint zero max packet size,
     * the descriptor will be sent in multiple ACCESSORY_SET_HID_REPORT_DESC
     * commands. The data for the descriptor must be sent sequentially
     * if multiple packets are needed.
     *
     * <https://source.android.com/devices/accessories/aoa2.html#hid-support>
     */
    // index (arg1): offset of data (buffer) in descriptor
    uint16_t offset = 0;
    while (offset < size) {
        uint16_t packet_length = size - offset;
        if (packet_length > max_packet_size_0) {
            packet_length = max_packet_size_0;
        }
        int r = libusb_control_transfer(handle, requestType, request, value,
                                        offset, buffer + offset, packet_length,
                                        timeout);
        offset += packet_length;
        if (r < 0) {
            print_libusb_error(r);
            return 1;
        }
    }

    return 0;
}

int send_hid_event(libusb_device_handle *handle, const unsigned char *event, uint16_t size) {
    const uint8_t requestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    const uint8_t request = AOA_SEND_HID_EVENT;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0 (unused)
    const uint16_t value = 0;
    const uint16_t index = 0;
    // libusb_control_transfer expects non-const but should not modify it
    unsigned char *const buffer = (unsigned char *) event;
    const uint16_t length = size;
    const unsigned int timeout = DEFAULT_TIMEOUT;
    int r = libusb_control_transfer(handle, requestType, request,
                                    value, index, buffer, length, timeout);
    if (r < 0) {
        print_libusb_error(r);
        return 1;
    }
    return 0;
}

int test_on_device(uint16_t vid, uint16_t pid) {
    libusb_init(NULL);

    libusb_device *device = find_device(vid, pid);
    if (!device) {
        fprintf(stderr, "Device %04x:%04x not found\n", vid, pid);
        return 1;
    }

    printf("Device %04x:%04x found. Opening...\n", vid, pid);

    int err = 0;
    libusb_device_handle *handle;
    int r = libusb_open(device, &handle);
    if (r) {
        print_libusb_error(r);
        libusb_unref_device(device);
        err = 1;
        goto finally_1;
    }

    printf("Registering HID...\n");
    if (register_hid(handle, REPORT_DESC_SIZE))
        goto finally_2;

    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(device, &desc);
    int max_packet_size_0 = desc.bMaxPacketSize0;

    printf("Sending HID descriptor...\n");
    if (send_hid_descriptor(handle, REPORT_DESC, REPORT_DESC_SIZE, max_packet_size_0))
        goto finally_2;

    // an event sent too early after the HID descriptor may fail
    usleep(100000);

    printf("Sending HID event...\n");
    if (send_hid_event(handle, HID_EVENT, HID_EVENT_SIZE))
        goto finally_2;

    printf("SUCCESS\n");

finally_2:
    libusb_close(handle);
finally_1:
    libusb_unref_device(device);
    return err;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        error(2, 0, "Usage: %s <vid> <pid>", argv[0]);
    }

    // parse vid and pid strings to uint16_t
    uint16_t vid;
    uint16_t pid;
    char *endptr;
    errno = 0;
    vid = strtol(argv[1], &endptr, 0x10);
    if (errno || *endptr)
        error(3, errno, "Cannot parse vid: %s", argv[1]);
    errno = 0;
    pid = strtol(argv[2], &endptr, 0x10);
    if (errno || *endptr)
        error(3, errno, "Cannot parse pid: %s", argv[2]);

    return test_on_device(vid, pid);
}
