# Remote Display for LVGL Development

This project provides a remote display tool to aid in the development of LVGL (Light and Versatile Graphics Library) applications. It allows you to control and test LVGL-based apps from your desktop, even if the hardware does not have a physical display attached during early development.

It's also great for taking screenshots or capturing videos for demonstration purposes without having to rely on separate storage or IO. This library can be used on a bare ESP32 development board with nothing attached other than power.

![Screenshot of the client interface](https://github.com/CubeCoders/LVGLRemoteServer/blob/main/ClientApp.png?raw=true)

## Features

- **Remote Display Control**: Control LVGL applications from your desktop.
- **RLE Packet Transmission**: Efficiently transmit Run-Length Encoded (RLE) packets.
- **UDP Communication**: Information is kept within single UDP packets for simplicity.
- **Touch Input Simulation**: Simulate touch input on the remote display.
- **Zero Allocation**: No dynamic allocation is performed, so the memory footprint is constant.

## Limitations

 - **16-bit RGB565 Color Only**: No other color depths are supported.
 - **No security or encryption**: This is absolutely not safe for use over public networks or in production. This is development tool only.

## Files

- `RemoteDisplay.cpp`: Implementation of the remote display functionalities.
- `RemoteDisplay.h`: Header file for the remote display class.

## Client application

Only available for Windows at this time, but it may run on Linux/MacOS via Mono. Download the client signed with our digital certificate here: [Client Application Link](https://downloads.cubecoders.com/LVGLRemote/Viewer_Signed.zip)

## Usage

### Arduino IDE users

Place `RemoteDisplay.cpp` and `RemoteDisplay.h` in a directory in your Arduino Libraries directory `Arduino/libraries/LVGLRemoteServer` and they will be available in any Arduino projects.

### Setup Example

```cpp

#include <lvgl.h>
#include <RemoteDisplay.h>

/* If you are configuring networking yourself, you can omit these parameters */
RemoteDisplay remoteDisplay("Wifi AP Name", "Wifi Password");

void disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixelmap) {
    remoteDisplay.sendRLE(area, pixelmap);

    /* Your existing disp_flush code */

    lv_disp_flush_ready(disp);
}

void setup()
{
    lv_init();
    lv_display_t* disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_buffers(disp, buf, NULL, SCREENBUFFER_SIZE_PIXELS * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /*If you have an existing disp_flush method, this doesn't change*/
    lv_display_set_flush_cb(disp, disp_flush);

    /*If you don't have a display and want to use the remote display only, you can use its internal handler*/
    //lv_display_set_flush_cb(disp, remoteDisplay.remoteDispFlush);

    remoteDisplay.init(disp);

    /*Optional: Initiate the connection from the server side so the display is sent to a client immediately.
      Useful with the clients kiosk mode for demonstrations or presentations. */
    remoteDisplay.connectRemote("192.168.100.50");
}
    
```

## Functions

- [`RemoteDisplay::init`](RemoteDisplay.cpp): Initialize, configure LVGL and optionally connect to WiFi.
- [`RemoteDisplay::send`](RemoteDisplay.cpp): Sends display data uncompressed (sometimes better for noisy data such as photos)
- [`RemoteDisplay::sendRLE`](RemoteDisplay.cpp): Sends display data RLE encoded.
- [`RemoteDisplay::remoteDispFlush`](RemoteDisplay.cpp): Flushes the display.

## Dependencies

- LVGL 9.x
- WiFi
- WiFiUdp

## License

This project is licensed under the MIT License. 
©2024 CubeCoders Limited
