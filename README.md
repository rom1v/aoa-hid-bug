# AOA-HID bug report sample

This sample project demonstrates that HID over AOA does not work on some
devices, typically many Samsung devices released in 2017, but also some others,
failing with a _pipe error_.

It attempts to send an HID mouse event to an Android device following the [AOA2
protocol].

[AOA2 protocol]: https://source.android.com/devices/accessories/aoa2#hid-support


## Build

Install the package `libusb-1.0-0-dev` (on _Debian_ or _Ubuntu_):

    sudo apt-get install libusb-1.0-0-dev

Build the sample:

    make


## Run

Find your Android device _vendor id_ and _product id_ in the output of `lsusb`:

    $ lsusb
    ...
    Bus 003 Device 016: ID 18d1:4ee1 Google Inc. Nexus 4
    ...

Here, the _vendor id_ is `18d1` and the _product id_ is `4ee1`.

Then run the sample providing these ids:

    ./hid 18d1 4ee1

The output should look like:

    $ ./hid 18d1 4ee1
    Device 18d1:4ee1 found. Opening...
    Registering HID...
    Sending HID descriptor...
    Sending HID event...
    SUCCESS

However, on some devices, sending the event does not work. For instance, here is
the result for a Lenovo TB3-850M:

    $ ./hid 17ef 79f1
    Device 17ef:79f1 found. Opening...
    Registering HID...
    Sending HID descriptor...
    Sending HID event...
    Pipe error

## Investigations

On failing devices, `dmesg` displays these lines immediately after
`send_hid_descriptor()`:

    (0)[8196:kworker/0:3]hid (null): transport driver missing .raw_request()
    (0)[8196:kworker/0:3][g_android]can't add hid device: -22
    (0)[8196:kworker/0:3][g_android]can't add HID deviceffffffc05bda8480

The error `transport driver missing .raw_request()` has been added by [commit
3c86726] (integrated since kernel `v3.15`).

[commit 3c86726]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=3c86726cfe38952f0366f86acfbbb025813ec1c2

In the kernel sources of SM-A320F (A3 2017) from [Samsung]
(`SM-A320F_CIS_MM_Opensource.zip`), we can see that they do not provide a
`raw_request` callback (`drivers/usb/gadget/function/f_accessory.c`):

[samsung]: http://opensource.samsung.com/

```c
static struct hid_ll_driver acc_hid_ll_driver = {
    .parse = acc_hid_parse,
    .start = acc_hid_start,
    .stop = acc_hid_stop,
    .open = acc_hid_open,
    .close = acc_hid_close,
};
```

The `raw_request` callback is not provided while the commit that made it
mandatory has been merged, hence the problem.

It should be provided like in [commit 975a683].

[commit 975a683]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=975a683271e690e7e467b274f22efadf1e696b5e
