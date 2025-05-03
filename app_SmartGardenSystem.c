#include "app.h"
#include <stdio.h>
#include <string.h>

#define MAX_ZONES 3
#define FREEZING_TEMP_F 32.0f   // Freezing temperature in Fahrenheit

/* Zone LEDs on PB0/PB1/PB2 */
#define ZONE_PORT GPIOB
static const uint16_t ZONE_PINS[MAX_ZONES] = {
    GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2
};

/* DHT11 on PC6 */
#define DHT_PORT GPIOC
#define DHT_PIN  GPIO_PIN_6

/* “Idle” red LED on PC7 */
#define RED_LED_PORT GPIOC
#define RED_LED_PIN  GPIO_PIN_7

extern UART_HandleTypeDef huart2;
static int zoneStates[MAX_ZONES] = {0};

/* Micro‐delay via DWT */
static void Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq()/1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

/* Bit‐bang DHT11: 0=OK, 1–5 error codes */
static uint8_t DHT11_Read(uint8_t *hum, uint8_t *tmp)
{
    uint8_t bits[5] = {0}, err;
    GPIO_InitTypeDef io = {0};
    uint32_t start, timeout = (HAL_RCC_GetHCLKFreq()/1000000) * 200;  // 200 µs

    /* 1) drive low 18 ms */
    io.Pin  = DHT_PIN;
    io.Mode = GPIO_MODE_OUTPUT_OD;
    io.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT_PORT, &io);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    Delay_us(18000);

    /* 2) release, wait 50 µs */
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    Delay_us(50);

    /* 3) input w/ pull‐up */
    io.Mode = GPIO_MODE_INPUT;
    io.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT_PORT, &io);

    __disable_irq();
    /* wait for sensor answer low */
    start = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(DHT_PORT,DHT_PIN)==GPIO_PIN_SET) {
        if ((DWT->CYCCNT - start) > timeout) { err=1; goto DONE; }
    }
    /* wait for sensor high */
    start = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(DHT_PORT,DHT_PIN)==GPIO_PIN_RESET) {
        if ((DWT->CYCCNT - start) > timeout) { err=2; goto DONE; }
    }
    /* wait for next low */
    start = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(DHT_PORT,DHT_PIN)==GPIO_PIN_SET) {
        if ((DWT->CYCCNT - start) > timeout) { err=3; goto DONE; }
    }

    /* read 40 bits */
    for (int i=0; i<40; i++) {
        /* wait low */
        start = DWT->CYCCNT;
        while (HAL_GPIO_ReadPin(DHT_PORT,DHT_PIN)==GPIO_PIN_RESET)
            if ((DWT->CYCCNT - start)>timeout) {err=4; goto DONE;}
        /* measure high */
        start = DWT->CYCCNT;
        while (HAL_GPIO_ReadPin(DHT_PORT,DHT_PIN)==GPIO_PIN_SET)
            if ((DWT->CYCCNT - start)>timeout*10) break;

        bits[i/8] <<= 1;
        if ((DWT->CYCCNT - start) > (HAL_RCC_GetHCLKFreq()/1000000 * 50))
            bits[i/8] |= 1;
    }

    /* checksum */
    if (bits[4] != (uint8_t)(bits[0]+bits[1]+bits[2]+bits[3])) {
        err = 5;
        goto DONE;
    }

    *hum = bits[0];
    *tmp = bits[2];
    err = 0;

DONE:
    __enable_irq();
    return err;
}

/* turn the red LED ON if idle (no zone), OFF otherwise */
static void updateRedIdle(void)
{
    for (int i=0; i<MAX_ZONES; i++)
        if (zoneStates[i]) {
            HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_RESET);
            return;
        }
    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_SET);
}

void App_Init(void)
{
    AllZonesOff();
    updateRedIdle();

    UART_TransmitString(&huart2, "==============================", 1);
    UART_TransmitString(&huart2, " Smart Garden Controller ",   1);
    UART_TransmitString(&huart2, "==============================", 1);
    DisplayStatus();
    ShowCommands();
}

void DisplayStatus(void)
{
    DisplayHumidity();
    for (int i=0; i<MAX_ZONES; i++) {
        char buf[32];
        sprintf(buf, "Zone %d: %s", i+1, zoneStates[i]? "ON":"OFF");
        UART_TransmitString(&huart2, buf, 1);
    }
    updateRedIdle();
}

void ShowCommands(void)
{
    UART_TransmitString(&huart2,
      "Smart Garden System Command Menu:", 1);
    UART_TransmitString(&huart2, "> 1-3: Zone Submenu",    1);
    UART_TransmitString(&huart2, "> A: All zones OFF",      1);
    UART_TransmitString(&huart2, "> S: Show Status",        1);
    UART_TransmitString(&huart2, "> T: LED Test",           1);
    UART_TransmitString(&huart2, "> U: Show Humidity/Temp", 1);
    UART_TransmitString(&huart2, "> F: Freeze Temp Alert",  1);  // Added freeze alert command
    UART_TransmitString(&huart2, "> H: Help Menu",          1);
}

void ZoneSubMenu(int zone)
{
    char buf[64];
    sprintf(buf, "--- Zone %d Submenu ---", zone+1);
    UART_TransmitString(&huart2, buf, 1);
    sprintf(buf, "  Status: %s", zoneStates[zone]? "ON":"OFF");
    UART_TransmitString(&huart2, buf, 1);
    UART_TransmitString(&huart2, "> O: ON",  1);
    UART_TransmitString(&huart2, "> F: OFF", 1);
    UART_TransmitString(&huart2, "> B: Back",1);

    char cmd;
    HAL_UART_Receive(&huart2, (uint8_t*)&cmd, 1, HAL_MAX_DELAY);
    if (cmd=='O'||cmd=='o') {
        /* only one on at once */
        for (int i=0;i<MAX_ZONES;i++)
            if (i!=zone && zoneStates[i]) {
                HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[i], GPIO_PIN_RESET);
                zoneStates[i]=0;
            }
        HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[zone], GPIO_PIN_SET);
        zoneStates[zone]=1;
    }
    else if (cmd=='F'||cmd=='f') {
        HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[zone], GPIO_PIN_RESET);
        zoneStates[zone]=0;
    }

    DisplayStatus();
}

void AllZonesOff(void)
{
    for (int i=0;i<MAX_ZONES;i++){
        HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[i], GPIO_PIN_RESET);
        zoneStates[i]=0;
    }
    updateRedIdle();
}

void DisplayHumidity(void)
{
    uint8_t h, t, err = DHT11_Read(&h, &t);
    char buf[64];

    // Check if the reading was successful
    if (err == 0) {
        // Display the humidity and temperature in Celsius
        sprintf(buf, "Humidity: %u%%  Temp: %u°C", h, t);
        UART_TransmitString(&huart2, buf, 1);

        // Flash red LED if temperature is near freezing (0°C)
        if (t <= 5) {  // If the temperature is near freezing (example: <= 5°C)
            // Flash the red LED to indicate freezing alert
            for (int i = 0; i < 5; i++) {
                HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_SET);  // Turn on red LED
                HAL_Delay(250);  // Wait for 250ms
                HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_RESET);  // Turn off red LED
                HAL_Delay(250);  // Wait for another 250ms
            }
        }
    } else {
        // If there's an error, report it
        sprintf(buf, "DHT11 error code: %u", err);
        UART_TransmitString(&huart2, buf, 1);
    }
}

void TestLEDs(void)
{
    AllZonesOff();
    HAL_Delay(200);
    for (int i=0;i<MAX_ZONES;i++){
        char buf[32];
        sprintf(buf, "Test: Zone %d", i+1);
        UART_TransmitString(&huart2, buf, 1);
        HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[i], GPIO_PIN_SET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[i], GPIO_PIN_RESET);
        HAL_Delay(200);
    }
    UART_TransmitString(&huart2,
      "Test complete: all ON 5s",1);
    for (int i=0;i<MAX_ZONES;i++)
        HAL_GPIO_WritePin(ZONE_PORT, ZONE_PINS[i], GPIO_PIN_SET);
    HAL_Delay(5000);
    AllZonesOff();
    UART_TransmitString(&huart2,
      "LED Test ended, all off",1);
}

void UART_TransmitString(UART_HandleTypeDef *huart,
                         char *s, int newline)
{
    HAL_UART_Transmit(huart,
      (uint8_t*)s, strlen(s), HAL_MAX_DELAY);
    if (newline)
        HAL_UART_Transmit(huart,
          (uint8_t*)"\r\n",2,HAL_MAX_DELAY);
}

void App_EmergencyStopISR(void)
{
    AllZonesOff();
    UART_TransmitString(&huart2, "EMERGENCY STOP: All zones OFF", 1);
}
