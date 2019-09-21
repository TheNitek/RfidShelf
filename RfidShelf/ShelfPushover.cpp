#include "ShelfPushover.h"
#ifdef PUSHOVER_ENABLE

ShelfPushover::ShelfPushover() {
}

unsigned int ShelfPushover::send(String message, int8_t priority, String sound) {

    String data = "user=";
    data += PUSHOVER_USER;
    data += "&token=";
    data += PUSHOVER_TOKEN;
    data += "&message=" + message;
    data += "&device=";
    data += PUSHOVER_DEVICE;
    data += "&priority=" + priority;
    data += "&sound=" + sound;

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    // do not validate certificate
    client->setInsecure();
    _httpClient.begin(*client, "https://api.pushover.net/1/messages.json");
    int httpCode = _httpClient.POST(data);
    // Sprint(F("pushover result: "));
    // Sprintln(httpCode);
    // Sprintln(_httpClient.getString());
    _httpClient.end();
    return httpCode;
}

bool ShelfPushover::sendPoweredNotification() {
    if (millis() - lastPoweredNotificationCheck > PUSHOVER_POWERED_NOTIFICATION_TIME) {
        lastPoweredNotificationCheck = millis();
        String message = PUSHOVER_RFIDSHELD_NAME;
        message += " still powered up";
        return send(message, 1, PUSHOVER_POWERED_SOUND) == 200;
    }
}

#endif
