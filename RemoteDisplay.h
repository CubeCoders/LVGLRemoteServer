#ifndef REMOTE_DISPLAY_H
#define REMOTE_DISPLAY_H

#include <lvgl.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define MAX_PACKET_SIZE 1430

class RemoteDisplay {
public:
    RemoteDisplay(int udpPort = 24680, const char* ssid = nullptr, const char* password = nullptr);
    RemoteDisplay(const char* ssid, const char* password);
    void init(lv_display_t *disp);
    void send(const lv_area_t *area, uint8_t *pixelmap);
    void sendRLE(const lv_area_t *area, uint8_t *pixelmap);
    void sendPalettedRLE(const lv_area_t *area, uint8_t *pixelmap); 
    void remoteDispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixelmap);
    void connectRemote(char *ipStr);
    void connectRemote(IPAddress ip);

private:
    WiFiUDP udp;
    uint32_t udpAddress;
    int udpPort;
    const char* ssid;
    const char* password;
    bool wifiConnected;
    uint8_t packetBuffer[MAX_PACKET_SIZE];
    uint16_t rleBuffer[MAX_PACKET_SIZE / 2];
    uint8_t infoBuffer[10];
    uint16_t paletteBuffer[16];

    void readRemoteTouch(lv_indev_t *indev_driver, lv_indev_data_t *data);
    void sendRLEPacket(uint16_t transmittedX, uint16_t transmittedY, uint16_t tileWidth, uint16_t progressStart, uint32_t rleLength, uint8_t paletteSize = 0);
    void transmitInfoPacket();
    void refreshDisplay();
};

#endif
