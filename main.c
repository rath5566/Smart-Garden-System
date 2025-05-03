#include "main.h"
#include "app.h"
#include <stdio.h>  // Include this header for sprintf

// Define the red LED port and pin (assuming PC7)
#define RED_LED_PORT GPIOC
#define RED_LED_PIN GPIO_PIN_7

UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);

// Emergency stop on user button (PC13)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        AllZonesOff();
        UART_TransmitString(&huart2, "EMERGENCY STOP: All zones OFF", 1);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* enable DWT for microsecond delays */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL     |= DWT_CTRL_CYCCNTENA_Msk;

    MX_GPIO_Init();
    MX_USART2_UART_Init();

    App_Init();               // prints header + initial status
    ShowCommands();

    uint32_t lastHum = HAL_GetTick();
    while (1)
    {
        char c;
        /* wait up to 500 ms for a key */
        if (HAL_UART_Receive(&huart2, (uint8_t*)&c, 1, 500) == HAL_OK)
        {
            switch (c)
            {
                case '1': case '2': case '3':
                    ZoneSubMenu(c - '1');
                    break;
                case 'A': case 'a':
                    AllZonesOff();
                    UART_TransmitString(&huart2, "All zones OFF", 1);
                    break;
                case 'S': case 's':
                    DisplayStatus();
                    break;
                case 'T': case 't':
                    TestLEDs();
                    break;
                case 'U': case 'u':
                    DisplayHumidity();
                    break;
                case 'F': case 'f':
                    // Trigger freezing temperature alert manually
                    UART_TransmitString(&huart2, "Freeze Temp Alert Triggered!\n", 1);
                    // Force a freezing temperature alert (set to 0°C for testing)
                    DisplayHumidityFreezeAlert(0); // Call the function to test freeze alert at 0°C
                    break;
                case 'H': case 'h':
                    /* redisplay menu */
                    break;
                default:
                    UART_TransmitString(&huart2, "Unknown command. Press H for help.", 1);
            }
            ShowCommands();
        }

        /* every 5 min auto‐update humidity/temperature */
        if ((HAL_GetTick() - lastHum) >= 300000U)
        {
            lastHum = HAL_GetTick();
            DisplayHumidity();
            ShowCommands();
        }
    }
}

void DisplayHumidityFreezeAlert(uint8_t t)
{
    char buf[64];

    // Display the humidity and temperature in Celsius (for testing, force 0°C)
    sprintf(buf, "Test Humidity: 40%%  Temp: %u°C", t);
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

        // Automatically turn off all zones when freeze alert is triggered
        AllZonesOff();  // Call the function to turn off all zones
        UART_TransmitString(&huart2, "Freeze Warning: All zones turned off.\n", 1);  // Notify the user
    }
}
1

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;
    RCC_OscInitStruct.PLL.PLLN            = 40;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4)!= HAL_OK)
        Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2)!= HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Zone LEDs PB0,1,2 */
    GPIO_InitStruct.Pin   = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB,
        GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2,
        GPIO_PIN_RESET);

    /* Red “idle” LED on PC7 */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);  // start “idle”

    /* DHT11 DATA on PC6: open‑drain + pull‑up */
    GPIO_InitStruct.Pin   = GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);

    /* User button PC13 -> EXTI */
    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
