#include "ShelfPushover.h"

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

    _httpClient.begin("http://api.pushover.net/1/messages.json");
    int httpCode = _httpClient.POST(data);
    // Serial.print(F("pushover result: "));
    // Serial.println(httpCode);
    // Serial.println(_httpClient.getString());
    _httpClient.end();
    return httpCode;
}

bool ShelfPushover::sendPoweredNotification() {
    String message = PUSHOVER_RFIDSHELD_NAME;
    message += " still powered up";
    return send(message, 1, PUSHOVER_POWERED_SOUND) == 200;
}