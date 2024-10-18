#include <Arduino.h>
#include "sMQTTBroker.h"
#include <Wire.h>              //Biblioteca utilizada gerenciar a comunicação entre dispositicos através do protocolo I2C
#include <LiquidCrystal_I2C.h> //Biblioteca controlar display 16x2 através do I2C
#include <WiFi.h>
#include "wifi_config.h"

sMQTTBroker broker;

IPAddress local_IP(10, 0, 100, 115);
IPAddress gateway(10, 0, 100, 1);
IPAddress subnet(255, 255, 255, 0);

void setup()
{

  Serial.begin(115200);

  if (!WiFi.config(local_IP, gateway, subnet))
  {
    Serial.println("STA Failed to configure");
  }

  const char *ssid = "WIFI_SSID";         // The SSID (name) of the Wi-Fi network you want to connect
  const char *password = "WIFI_PASSWORD"; // The password of the Wi-Fi network

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  const unsigned short mqttPort = 1883;
  broker.init(mqttPort);
  // all done
}
void loop()
{
  broker.update();
}