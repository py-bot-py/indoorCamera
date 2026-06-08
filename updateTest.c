#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

const char* versionURL = "http://192.168.1.100:8000/version.txt";
const char* firmwareURL = "http://192.168.1.100:8000/firmware.bin";

const int CURRENT_VERSION = 1;

void performOTA()
{
    HTTPClient http;

    http.begin(versionURL);

    int code = http.GET();

    if (code == 200)
    {
        int remoteVersion = http.getString().toInt();

        Serial.printf("Current=%d Remote=%d\n",
                      CURRENT_VERSION,
                      remoteVersion);

        if (remoteVersion > CURRENT_VERSION)
        {
            Serial.println("Update available");

            HTTPClient updateHttp;
            updateHttp.begin(firmwareURL);

            int updateCode = updateHttp.GET();

            if (updateCode == 200)
            {
                int contentLength = updateHttp.getSize();
                WiFiClient* stream = updateHttp.getStreamPtr();

                if (Update.begin(contentLength))
                {
                    size_t written =
                        Update.writeStream(*stream);

                    if (written == contentLength)
                    {
                        Serial.println("Firmware downloaded");
                    }

                    if (Update.end() && Update.isFinished())
                    {
                        Serial.println("Update successful");
                        ESP.restart();
                    }
                }
            }

            updateHttp.end();
        }
        else
        {
            Serial.println("Already up to date");
        }
    }

    http.end();
}

void setup()
{
    Serial.begin(115200);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected");

    performOTA();
}

void loop()
{
    static int counter = 0;

    Serial.printf("Counter = %d\n", counter++);

    delay(1000);
}