#include <Arduino.h>
#include "sMQTTBroker.h"
#include <Wire.h>              //Biblioteca utilizada gerenciar a comunicação entre dispositicos através do protocolo I2C
#include <LiquidCrystal_I2C.h> //Biblioteca controlar display 16x2 através do I2C
#include <WiFi.h>
#include "wifi_config.h"
#include "dataCollector.h"

#define col 16    // Define o número de colunas do display utilizado
#define lin 2     // Define o número de linhas do display utilizado
#define ende 0x27 // Define o endereço do display

LiquidCrystal_I2C lcd(ende, col, lin); // Cria o objeto lcd passando como parâmetros o endereço, o nº de colunas e o nº de linhas

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

  const char *ssid = WIFI_SSID;         // The SSID (name) of the Wi-Fi network you want to connect
  const char *password = WIFI_PASSWORD; // The password of the Wi-Fi network

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  lcd.init();      // Inicializa a comunicação com o display já conectado
  lcd.clear();     // Limpa a tela do display
  lcd.backlight(); // Aciona a luz de fundo do display
  // const unsigned short mqttPort = 1883;
  // broker.init(mqttPort);
  // all done
}
void loop()
{
  // broker.update();
  float temperatura = coletarTemperatura();
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");
  lcd.setCursor(0, 0);                               // Coloca o cursor do display na coluna 1 e linha 1
  lcd.print("Temp.: " + (String)temperatura + " C"); // Exibe a mensagem na primeira linha do display
  // lcd.setCursor(0, 1);           // Coloca o cursor do display na coluna 1 e linha 2
  // lcd.print("TUTORIAL DISPLAY"); // Exibe a mensagem na segunda linha do display
  delay(2000); // Envia a cada 5 segundos
}