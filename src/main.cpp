#include <Arduino.h>
#include <Wire.h>              // Biblioteca para comunicação I2C
#include <LiquidCrystal_I2C.h> // Biblioteca para controlar o display LCD via I2C
#include <WiFi.h>
#include "wifi_config.h"
#include "dataCollector.h"

// Bibliotecas adicionais para MQTT e Azure IoT
#include <cstdlib>
#include <string.h>
#include <time.h>
#include <mqtt_client.h>
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>
#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "iot_configs.h"

// Configurações do LCD
#define col 16    // Número de colunas do display
#define lin 2     // Número de linhas do display
#define ende 0x27 // Endereço do display LCD

LiquidCrystal_I2C lcd(ende, col, lin);

// Configurações de MQTT e Azure IoT
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define TELEMETRY_FREQUENCY_MILLISECS 5000
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define PST_TIME_ZONE -8
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1
#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)

// Variáveis globais para Wi-Fi, MQTT e Azure IoT
static const char *ssid = IOT_CONFIG_WIFI_SSID;
static const char *password = IOT_CONFIG_WIFI_PASSWORD;
static const char *host = IOT_CONFIG_IOTHUB_FQDN;
static const char *mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char *device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;

static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;

static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];
static unsigned long next_telemetry_send_time_ms = 0;
static char telemetry_topic[128];
static String telemetry_payload = "{}";

#ifndef IOT_CONFIG_USE_X509_CERT
static AzIoTSasToken sasToken(
    &client,
    AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),
    AZ_SPAN_FROM_BUFFER(sas_signature_buffer),
    AZ_SPAN_FROM_BUFFER(mqtt_password));
#endif

int PinoRele = 26;

// Funções auxiliares
static void connectToWiFi()
{
  Logger.Info("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

static void initializeTime()
{
  Logger.Info("Setting time using SNTP...");
  configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST, NTP_SERVERS);
  time_t now = time(NULL);
  while (now < 1510592825) // Tempo base (13 de novembro de 2017)
  {
    delay(500);
    now = time(nullptr);
  }
  Logger.Info("Time synchronized.");
}

static void initializeIoTHubClient()
{
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  if (az_result_failed(az_iot_hub_client_init(&client, az_span_create((uint8_t *)host, strlen(host)),
                                              az_span_create((uint8_t *)device_id, strlen(device_id)), &options)))
  {
    Logger.Error("Failed to initialize IoT Hub client.");
    return;
  }

  az_iot_hub_client_get_client_id(&client, mqtt_client_id, sizeof(mqtt_client_id), NULL);
  az_iot_hub_client_get_user_name(&client, mqtt_username, sizeof(mqtt_username), NULL);

  Logger.Info("IoT Hub client initialized.");
}

static int initializeMqttClient()
{
#ifndef IOT_CONFIG_USE_X509_CERT
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0)
  {
    Logger.Error("Failed generating SAS token.");
    return 1;
  }
#endif

  esp_mqtt_client_config_t mqtt_config = {};
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;
  mqtt_config.password = (const char *)az_span_ptr(sasToken.Get());
  mqtt_config.cert_pem = (const char *)ca_pem;

  mqtt_client = esp_mqtt_client_init(&mqtt_config);
  if (mqtt_client == NULL)
  {
    Logger.Error("Failed creating MQTT client.");
    return 1;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);
  if (start_result != ESP_OK)
  {
    Logger.Error("Failed starting MQTT client.");
    return 1;
  }

  Logger.Info("MQTT client initialized.");
  return 0;
}

static void generateTelemetryPayload(float temperature, float ph)
{
  // gera o payload com a temperatura em valor arredondado em inteiro e tambem o ph com uma casa decimal
  telemetry_payload = "{ \"temperatura\": " + String((int)round(temperature)) + ", \"ph\": " + String(ph, 1) + " }";
}

static void sendTelemetry(float temperature, float ph)
{
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(&client, NULL, telemetry_topic, sizeof(telemetry_topic), NULL)))
  {
    Logger.Error("Failed to get telemetry topic.");
    return;
  }

  generateTelemetryPayload(temperature, ph);

  if (esp_mqtt_client_publish(mqtt_client, telemetry_topic, telemetry_payload.c_str(), telemetry_payload.length(),
                              MQTT_QOS1, DO_NOT_RETAIN_MSG) == 0)
  {
    Logger.Error("Failed publishing telemetry.");
  }
  else
  {
    Logger.Info("Telemetry sent: " + telemetry_payload);
  }
}

// Setup e loop principais
void setup()
{
  Serial.begin(115200);
  lcd.init();
  lcd.clear();
  lcd.backlight();

  pinMode(PinoRele, OUTPUT);

  connectToWiFi();
  initializeTime();
  initializeIoTHubClient();
  initializeMqttClient();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectToWiFi();
  }

  float temperatura = coletarTemperatura();
  float ph = coletarPh(); // Valor do sensor de pH
  // float ph = 7.1; // Valor fixo simulando o sensor de pH

  // ligar rele se temperatura maior que 25 graus
  if (temperatura > 25)
  {
    digitalWrite(PinoRele, LOW);
  }
  else
  {
    digitalWrite(PinoRele, HIGH);
  }

  sendTelemetry(temperatura, ph);

  // Exibição no LCD
  lcd.setCursor(0, 0);
  lcd.print("Temp: " + String((int)temperatura) + " C");
  lcd.setCursor(0, 1);
  lcd.print("pH: " + String(ph, 1));

  delay(2000); // Intervalo entre leituras
}
