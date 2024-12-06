#include "dataCollector.h"
#include <Arduino.h>
#include <math.h> // Para a função log()

// Definições de pinos e constantes para o sensor NTC10K
int PinoNTC = 32;       // PINO DO NTC10K
int PinoPh = 34;        // PINO DO SENSOR PH
double Vs = 3.3;        // TENSÃO DE SAÍDA DO ESP32
double R1 = 10000;      // RESISTOR UTILIZADO NO DIVISOR DE TENSÃO
double Beta = 3950;     // VALOR DE BETA
double To = 298.15;     // VALOR EM KELVIN REFERENTE A 25° CELSIUS
double Ro = 10000;      // RESISTÊNCIA DO NTC A 25°C
double adcMax = 4095.0; // Valor máximo de ADC para ESP32

// variáveis para cálculo do ph
int buf[10];

// Implementação da função para coletar a temperatura
float coletarTemperatura()
{
    // Variáveis para cálculos
    double Vout, Rt = 0;
    double T, Tc, adc = 0;

    // Leitura do valor analógico do sensor NTC10K
    adc = analogRead(PinoNTC);

    // Cálculos para conversão da leitura em temperatura (em °C)
    Vout = adc * Vs / adcMax;
    Rt = R1 * Vout / (Vs - Vout);
    T = 1 / (1 / To + log(Rt / Ro) / Beta); // Conversão para Kelvin
    Tc = T - 273.15;                        // Conversão de Kelvin para Celsius

    return Tc; // Retorna a temperatura em °C
}

float coletarPh()
{
    int measure = analogRead(PinoPh);
    // Serial.print("Measure: ");
    // Serial.print(measure);

    double voltage = 5 / 1024.0 * measure; // classic digital to voltage conversion
    // Serial.print("\tVoltage: ");
    // Serial.print(voltage, 3);

    // PH_step = (voltage@PH7 - voltage@PH4) / (PH7 - PH4)
    // PH_probe = PH7 - ((voltage@PH7 - voltage@probe) / PH_step)
    float Po = 7 + ((2.5 - voltage) / 0.18);
    // Serial.print("\tPH: ");
    // Serial.print(Po, 3);
    return Po;

    // Serial.println("");
    // delay(2000);
}