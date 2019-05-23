#ifndef ShelfPushover_h
#define ShelfPushover_h

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "ShelfConfig.h"

#ifdef PUSHOVER_ENABLE
class ShelfPushover {
    public:
        ShelfPushover();
        unsigned int send(String message, int8_t priority = 0, String sound = "");
        bool sendPoweredNotification();
        
    private:
        HTTPClient _httpClient;
};
#endif
#endif