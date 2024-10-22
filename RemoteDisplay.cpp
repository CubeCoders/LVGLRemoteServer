#include "RemoteDisplay.h"

RemoteDisplay::RemoteDisplay(int udpPort, const char* ssid, const char* password)
    : udpPort(udpPort), ssid(ssid), password(password), udpAddress(0), wifiConnected(false) {}

RemoteDisplay::RemoteDisplay(const char* ssid, const char* password)
    : udpPort(24680), ssid(ssid), password(password), udpAddress(0), wifiConnected(false) {}

void RemoteDisplay::init(lv_display_t *disp) {
    if (ssid != nullptr && password != nullptr) {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(100);
            Serial.print(".");
        }
        Serial.println("");
        Serial.println("WiFi connected.");
        Serial.print("IP Address:");
        Serial.println(WiFi.localIP());
        wifiConnected = true;
    }

    udp.begin(udpPort);

    static lv_indev_t *remotedev = lv_indev_create();
    lv_indev_set_type(remotedev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(remotedev, [](lv_indev_t* indev, lv_indev_data_t* data){
        RemoteDisplay* instance = static_cast<RemoteDisplay*>(lv_indev_get_user_data(indev));
        instance->readRemoteTouch(indev, data);
    });

    lv_indev_set_user_data(remotedev, this);
}

void RemoteDisplay::connectRemote(char *ipStr) {
    IPAddress ip;
    if (ip.fromString(ipStr)) {
        connectRemote(ip);
    }
}

void RemoteDisplay::connectRemote(IPAddress ip) {
    udpAddress = ip;
    transmitInfoPacket();
    refreshDisplay();
}

void RemoteDisplay::send(const lv_area_t *area, uint8_t *pixelmap) {
    if (udpAddress == 0)
    {
        return;
    }
    uint32_t fullWidth = (area->x2 - area->x1 + 1); // Full width of the display area
    uint32_t fullHeight = (area->y2 - area->y1 + 1);
    size_t bytesPerPixel = 2; // 16bpp (RGB565 format)

    // Serial.printf("Transmitting area X:%d, Y:%d %dx%d\n", area->x1, area->y1, fullWidth, fullHeight);

    // Tile size: 32x16 pixels (each tile is 1024 bytes of pixel data)
    const uint32_t tileWidth = 40;
    const uint32_t tileHeight = 16;

    // Calculate the number of tiles in X and Y directions
    uint32_t numTilesX = (fullWidth + tileWidth - 1) / tileWidth; // Ceiling division
    uint32_t numTilesY = (fullHeight + tileHeight - 1) / tileHeight;

    // Loop through the number of tiles
    for (uint32_t tileIndexY = 0; tileIndexY < numTilesY; tileIndexY++)
    {
        for (uint32_t tileIndexX = 0; tileIndexX < numTilesX; tileIndexX++)
        {
            // Calculate the top-left corner of the tile
            uint32_t tileX = tileIndexX * tileWidth;
            uint32_t tileY = tileIndexY * tileHeight;

            // Calculate the actual tile width and height safely
            uint32_t actualTileWidth = min(tileWidth, fullWidth - tileX);
            uint32_t actualTileHeight = min(tileHeight, fullHeight - tileY);

            // Calculate the size of the tile's pixel data
            size_t actualTileDataSize = actualTileWidth * actualTileHeight * bytesPerPixel;

            uint16_t controlValue = 0x00;
            uint16_t sendX = (area->x1 + tileX);
            uint16_t sendY = (area->y1 + tileY);
            uint16_t sendW = actualTileWidth;
            uint16_t sendH = actualTileHeight;

            // Create buffer to hold position, size, and raw pixel data
            size_t packetSize = sizeof(controlValue) + sizeof(sendX) + sizeof(sendY) + sizeof(sendW) + sizeof(sendH);
            size_t totalPacketSize = packetSize + actualTileDataSize;

            memcpy(packetBuffer, &controlValue, sizeof(controlValue));                                                          // transmitted X position
            memcpy(packetBuffer + sizeof(controlValue), &sendX, sizeof(sendX));                                                 // transmitted X position
            memcpy(packetBuffer + sizeof(controlValue) + sizeof(sendX), &sendY, sizeof(sendY));                                 // transmitted Y position
            memcpy(packetBuffer + sizeof(controlValue) + sizeof(sendX) + sizeof(sendY), &sendW, sizeof(sendW));                 // width
            memcpy(packetBuffer + sizeof(controlValue) + sizeof(sendX) + sizeof(sendY) + sizeof(sendW), &sendH, sizeof(sendH)); // height

            // Now we need to extract the pixel data for this tile from the full pixelmap
            uint8_t *tileDataPtr = packetBuffer + packetSize;

            for (uint32_t row = 0; row < actualTileHeight; row++)
            {
                // Calculate the start of the current row in the full image
                size_t srcOffset = (tileY + row) * fullWidth * bytesPerPixel + (tileX * bytesPerPixel);
                size_t destOffset = row * actualTileWidth * bytesPerPixel;

                // Copy the correct number of bytes for each row
                memcpy(tileDataPtr + destOffset, pixelmap + srcOffset, actualTileWidth * bytesPerPixel);
            }

            // Send the tile via UDP
            udp.beginPacket(udpAddress, udpPort);
            udp.write(packetBuffer, totalPacketSize);
            udp.endPacket();
        }
    }
}

int getPaletteIndex(uint16_t* palette, int paletteSize, uint16_t color) {
    for (int i = 0; i < paletteSize; i++) {
        if (palette[i] == color) {
            return i;  // Return the index if color is found
        }
    }
    return 0;
}

void RemoteDisplay::sendPalettedRLE(const lv_area_t *area, uint8_t *pixelmap) {
    const int maxPaletteSize = 16; // Hardcoded limit
    int paletteSize = 0;
    uint32_t fullWidth = area->x2 - area->x1 + 1;
    uint32_t fullHeight = area->y2 - area->y1 + 1;
    uint32_t totalPixels = fullWidth * fullHeight;
    size_t bytesPerPixel = 2; // 16bpp (RGB565 format)

    // Traverse the pixel data and build the palette using paletteBuffer
    for (uint32_t pixel = 0; pixel < totalPixels; pixel++) {
        uint16_t color = pixelmap[pixel * bytesPerPixel] | (pixelmap[pixel * bytesPerPixel + 1] << 8);

        // Check if color is already in the palette
        bool found = false;
        for (int i = 0; i < paletteSize; i++) {
            if (paletteBuffer[i] == color) {
                found = true;
                break;
            }
        }

        // Add the color to the palette if not found
        if (!found) {
            if (paletteSize < maxPaletteSize) {
                paletteBuffer[paletteSize++] = color;
            } else {
                // Exceeded palette size, fall back to standard RLE
                sendRLE(area, pixelmap);
                return;
            }
        }
    }

    uint32_t rleCount = 0;  // Number of RLE entries in the buffer
    uint32_t rleLength = 0;  // Index for the rleBuffer
    uint16_t runLength = 1; // Current run length
    uint32_t totalPixelsProcessed = 0;
    uint32_t runDataPosition = 0;
    uint32_t pixelsInPacket = 0;

    memcpy(rleBuffer, paletteBuffer, paletteSize * 2);
    rleLength = paletteSize;
    const size_t maxRLEEntries = (MAX_PACKET_SIZE - 10 - (paletteSize * sizeof(uint16_t))) / 2; // Each RLE entry is 2 bytes - 10 byte header - 2 bytes per palette entry

    // Initialize lastColor with the first pixel color
    uint16_t lastColor = pixelmap[0] | (pixelmap[1] << 8);

    // Traverse through the pixel data
    for (uint32_t pixel = 1; pixel <= totalPixels; pixel++)
    {
        uint16_t color = 0;
        uint8_t colorPaletteIndex = 0;
        if (pixel < totalPixels)
        {
            uint32_t pixelIndex = pixel * bytesPerPixel;
            color = pixelmap[pixelIndex] | (pixelmap[pixelIndex + 1] << 8);
            colorPaletteIndex = getPaletteIndex(paletteBuffer, paletteSize, color);
        }

        bool colorChanged = (pixel == (totalPixels - 1)) || (color != lastColor);
        bool runLengthMaxed = (runLength == 255);
        bool bufferFull = (rleCount == maxRLEEntries);

        if (colorChanged || runLengthMaxed || bufferFull)
        {
            // Add the current run to the RLE buffer
            rleBuffer[rleLength++] = (colorPaletteIndex << 8) | runLength;
            rleCount++;
            totalPixelsProcessed += runLength;
            pixelsInPacket += runLength;

            // Send the RLE buffer if the buffer is full or if we've processed all pixels
            if (bufferFull || pixel == totalPixels)
            {
                sendRLEPacket(area->x1, area->y1, fullWidth, runDataPosition, rleLength, paletteSize);
                runDataPosition = totalPixelsProcessed;
                rleLength = paletteSize;
                rleCount = 0;
                pixelsInPacket = 0;
            }

            // Reset runLength and update lastColor
            runLength = 1;
            lastColor = color;
        }
        else
        {
            // Continue the current run
            runLength++;
        }
    }

    // Check if the total number of pixels sent matches the expected totalPixels
    if (totalPixelsProcessed != totalPixels)
    {
        Serial.printf("Discrepancy: Expected %d pixels, but sent %d pixels\n", totalPixels, totalPixelsProcessed);
    }
    else
    {
        //        Serial.println("All pixels sent successfully");
    }
}


void RemoteDisplay::sendRLE(const lv_area_t *area, uint8_t *pixelmap) {
    uint32_t fullWidth = (area->x2 - area->x1 + 1);
    uint32_t fullHeight = (area->y2 - area->y1 + 1);
    uint32_t totalPixels = fullWidth * fullHeight;
    size_t bytesPerPixel = 2; // 16bpp (RGB565 format)

    const size_t maxRLEEntries = (MAX_PACKET_SIZE - 10) / 4; // Each RLE entry is 4 bytes - 10 byte header

    // Serial.printf("Transmitting area X:%d, Y:%d %dx%d - %d pixels\n", area->x1, area->y1, fullWidth, fullHeight, totalPixels);

    uint32_t rleCount = 0;  // Number of RLE entries in the buffer
    uint32_t rleLength = 0;  // Index for the rleBuffer
    uint16_t runLength = 1; // Current run length
    uint32_t totalPixelsProcessed = 0;
    uint32_t runDataPosition = 0;
    uint32_t pixelsInPacket = 0;

    // Initialize lastColor with the first pixel color
    uint16_t lastColor = pixelmap[0] | (pixelmap[1] << 8);

    // Traverse through the pixel data
    for (uint32_t pixel = 1; pixel <= totalPixels; pixel++)
    {
        uint16_t color = 0;
        if (pixel < totalPixels)
        {
            uint32_t pixelIndex = pixel * bytesPerPixel;
            color = pixelmap[pixelIndex] | (pixelmap[pixelIndex + 1] << 8);
        }

        bool colorChanged = (pixel == (totalPixels - 1)) || (color != lastColor);
        bool runLengthMaxed = (runLength == 65535);
        bool bufferFull = (rleCount == maxRLEEntries);

        if (colorChanged || runLengthMaxed || bufferFull)
        {
            // Add the current run to the RLE buffer
            rleBuffer[rleLength++] = lastColor;
            rleBuffer[rleLength++] = runLength;
            rleCount++;
            totalPixelsProcessed += runLength;
            pixelsInPacket += runLength;

            // Send the RLE buffer if the buffer is full or if we've processed all pixels
            if (bufferFull || pixel == totalPixels)
            {
                sendRLEPacket(area->x1, area->y1, fullWidth, runDataPosition, rleLength);
                runDataPosition = totalPixelsProcessed;
                rleLength = 0;
                rleCount = 0;
                pixelsInPacket = 0;
            }

            // Reset runLength and update lastColor
            runLength = 1;
            lastColor = color;
        }
        else
        {
            // Continue the current run
            runLength++;
        }
    }

    // Check if the total number of pixels sent matches the expected totalPixels
    if (totalPixelsProcessed != totalPixels)
    {
        Serial.printf("Discrepancy: Expected %d pixels, but sent %d pixels\n", totalPixels, totalPixelsProcessed);
    }
    else
    {
        //        Serial.println("All pixels sent successfully");
    }
}

void RemoteDisplay::sendRLEPacket(uint16_t transmittedX, uint16_t transmittedY, uint16_t tileWidth, uint16_t progressStart, uint32_t rleLength, uint8_t paletteSize) {
    if (udpAddress == 0) { return; }

    static bool firstPacket = true;

    const uint16_t controlValue = paletteSize == 0 ? 0x0001 : 0x0100 | paletteSize;

    if (firstPacket && paletteSize > 0) {
        //Log all the params to serial for debugging
        Serial.printf("Transmitting RLE packet: X:%d, Y:%d, Width:%d, Progress:%d, Length:%d, PaletteSize:%d\n", transmittedX, transmittedY, tileWidth, progressStart, rleLength, paletteSize);
        //Also log the palette data with indicies as hex
        Serial.print("Palette: ");
        for (int i = 0; i < paletteSize; i++) {
            Serial.printf("%04X ", paletteBuffer[i]);
        }
        Serial.println();
        firstPacket = false;
    }

    // Create buffer to hold position, size, and raw pixel data
    size_t packetSize = sizeof(controlValue) + sizeof(transmittedX) + sizeof(transmittedY) + sizeof(tileWidth) + sizeof(progressStart);
    size_t totalPacketSize = packetSize + rleLength * sizeof(uint16_t);

    memcpy(packetBuffer, &controlValue, sizeof(controlValue));                                                                                            // Control value
    memcpy(packetBuffer + sizeof(controlValue), &transmittedX, sizeof(transmittedX));                                                                     // X
    memcpy(packetBuffer + sizeof(controlValue) + sizeof(transmittedX), &transmittedY, sizeof(transmittedY));                                              // Y
    memcpy(packetBuffer + sizeof(controlValue) + sizeof(transmittedX) + sizeof(transmittedY), &tileWidth, sizeof(tileWidth));                             // Width
    memcpy(packetBuffer + sizeof(controlValue) + sizeof(transmittedX) + sizeof(transmittedY) + sizeof(tileWidth), &progressStart, sizeof(progressStart)); // Progress (start of packet)

    // Copy RLE-encoded pixel data into the packet buffer
    memcpy(packetBuffer + packetSize, rleBuffer, rleLength * sizeof(uint16_t));

    // Send the packet via UDP
    udp.beginPacket(udpAddress, udpPort);
    udp.write(packetBuffer, totalPacketSize);
    udp.endPacket();
}

void RemoteDisplay::transmitInfoPacket() {
        // Following the same packet format as before, use a control value of 0x0002 to indicate an info packet
    // Containing the screen dimensions with the remaining two 16-bit values being empty.
    const uint16_t controlValue = 0xFFFF;
    uint16_t screenWidth = (uint16_t)LV_HOR_RES;
    uint16_t screenHeight = (uint16_t)LV_VER_RES;

    size_t packetSize = sizeof(controlValue) + sizeof(screenWidth) + sizeof(screenHeight) + sizeof(uint16_t) + sizeof(uint16_t);

    memcpy(infoBuffer, &controlValue, sizeof(controlValue));                                                         // Control value
    memcpy(infoBuffer + sizeof(controlValue), &screenWidth, sizeof(screenWidth));                                    // Screen width
    memcpy(infoBuffer + sizeof(controlValue) + sizeof(screenWidth), &screenHeight, sizeof(screenHeight));            // Screen height
    memset(infoBuffer + sizeof(controlValue) + sizeof(screenWidth) + sizeof(screenHeight), 0, sizeof(uint16_t) * 2); // Remaining two 16-bit values

    // Send the packet via UDP
    udp.beginPacket(udpAddress, udpPort);
    udp.write(infoBuffer, packetSize);
    udp.endPacket();
}

void RemoteDisplay::refreshDisplay() {
    lv_area_t area;
    area.x1 = 0;
    area.y1 = 0;
    area.x2 = LV_HOR_RES;
    area.y2 = LV_VER_RES;
    
    lv_obj_invalidate_area(lv_scr_act(), &area); // Invalidate this region on the active screen
}

void RemoteDisplay::readRemoteTouch(lv_indev_t *indev_driver, lv_indev_data_t *data) {
    uint8_t remoteStatus = 0;
    static lv_indev_state_t lastRemoteTouchState = LV_INDEV_STATE_REL;
    static uint16_t lastRemoteTouchX = 0;
    static uint16_t lastRemoteTouchY = 0;
    char incomingPacket[5]; // 1 byte status + 2x 16-bit integers for X and Y
    int packetSize = udp.parsePacket();
    if (packetSize == 5)
    { // We expect a 5-byte packet
        udp.read(incomingPacket, 5);

        // Extract touch data from packet
        remoteStatus = incomingPacket[0];
        int16_t remoteX = (incomingPacket[1] << 8) | incomingPacket[2]; // High byte first
        int16_t remoteY = (incomingPacket[3] << 8) | incomingPacket[4]; // High byte first

        switch (remoteStatus)
        {
            case 0: //Release
                lastRemoteTouchState = LV_INDEV_STATE_REL;
                break;
            case 1: //Touch
                lastRemoteTouchState = LV_INDEV_STATE_PR;
                lastRemoteTouchX = remoteX;
                lastRemoteTouchY = remoteY;
                break;
            case 2: // Refresh the screen - also used to connect to the remote display
                // Get the IP address as a string
                char ipStr[16];
                sprintf(ipStr, "%d.%d.%d.%d", udp.remoteIP()[0], udp.remoteIP()[1], udp.remoteIP()[2], udp.remoteIP()[3]);
                Serial.printf("Received connect/refresh command from %s\n", ipStr);

                udpAddress = udp.remoteIP();
                transmitInfoPacket();
                refreshDisplay();
                break;
        }
    }

    data->state = lastRemoteTouchState;
    data->point.x = lastRemoteTouchX;
    data->point.y = lastRemoteTouchY;
}

void RemoteDisplay::remoteDispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixelmap) {
    sendRLE(area, pixelmap);
    lv_disp_flush_ready(disp);
}
