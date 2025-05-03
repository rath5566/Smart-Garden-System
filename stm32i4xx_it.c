#include "stm32l4xx_hal.h"
#include "app.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* user push‑button PC13 -> EXTI line 13 */
void EXTI15_10_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_13) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
        App_EmergencyStopISR();
    }
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
}
