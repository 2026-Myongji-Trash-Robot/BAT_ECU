/**
 ******************************************************************************
 * @file    ina228_example.c
 * @brief   Example usage of the INA228 driver (struct-based, ReadAll API)
 * @note    Reference code — copy into your CubeMX-generated main.c
 ******************************************************************************
 */

#include "main.h"
#include "ina228.h"

/* Declared by CubeMX */
extern I2C_HandleTypeDef hi2c1;

/* ============================================================== */
/*  Single struct holds ALL measurements — no more separate vars   */
/* ============================================================== */
static INA228_Data g_ina;

void App_Init(void)
{
    if (INA228_Init(&hi2c1) != INA228_OK) {
        Error_Handler();
    }

    /* Optional: start SoC tracking from a clean accumulator */
    INA228_ResetAccumulators();
}

/* Called from a 10 ms timer or main loop */
void App_PollINA228(void)
{
    if (INA228_ReadAll(&g_ina) != INA228_OK) {
        /* TODO: handle I2C failure (retry / fault flag / log) */
        return;
    }

    /* Access measurements directly from the struct:
     *
     *   g_ina.bus_voltage_V     (V)
     *   g_ina.shunt_voltage_mV  (mV)
     *   g_ina.current_A         (A)
     *   g_ina.power_W           (W)
     *   g_ina.die_temp_C        (°C)
     *   g_ina.charge_C          (Coulombs — feed into SoC layer)
     *   g_ina.energy_J          (Joules)
     *
     * Hand off to:
     *   - CAN module → CAN_SendBatteryStatus(&g_ina)
     *   - SoC module → BMS_SoC_Update(g_ina.charge_C, g_ina.bus_voltage_V, g_ina.die_temp_C)
     *   - Logger    → SD card / UART
     */
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    App_Init();

    uint32_t last_tick = HAL_GetTick();
    while (1) {
        if (HAL_GetTick() - last_tick >= 10) {  /* 10ms cycle */
            last_tick = HAL_GetTick();
            App_PollINA228();
            /* CAN_SendBatteryStatus(&g_ina); */
        }
    }
}
