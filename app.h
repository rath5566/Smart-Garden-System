#ifndef __APP_H
#define __APP_H

#include "main.h"

/* Function declarations */
void AllZonesOff(void);
void UART_TransmitString(UART_HandleTypeDef *huart, char *s, int newline);
void DisplayStatus(void);
void ShowCommands(void);
void ZoneSubMenu(int zone);
void DisplayHumidity(void);
void TestLEDs(void);
void App_EmergencyStopISR(void);
void App_Init(void);
void DisplayHumidityFreezeAlert(uint8_t t);

#endif /* __APP_H */
