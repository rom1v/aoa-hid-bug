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
