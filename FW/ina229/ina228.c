/**
 ******************************************************************************
 * @file    ina228.c
 * @brief   INA228 driver implementation (STM32 HAL, polling, single device)
 ******************************************************************************
 */

#include "ina228.h"
#include <string.h>

/* ========================================================================== */
/*                            PRIVATE STATE                                    */
/* ========================================================================== */
static I2C_HandleTypeDef *s_hi2c = NULL;

/* ========================================================================== */
/*                       LOW-LEVEL I2C HELPERS                                 */
/* ========================================================================== */

/* Write a 16-bit register (MSB first per datasheet §7.5.1). */
static INA228_Status ina228_write16(uint8_t reg, uint16_t value)
{
    if (s_hi2c == NULL) return INA228_ERR_PARAM;

    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);

    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(
        s_hi2c,
        INA228_I2C_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        buf,
        2U,
        INA228_I2C_TIMEOUT_MS);

    if (st == HAL_TIMEOUT) return INA228_ERR_TIMEOUT;
    if (st != HAL_OK)      return INA228_ERR_I2C;
    return INA228_OK;
}

/* Read a 16-bit register. */
static INA228_Status ina228_read16(uint8_t reg, uint16_t *value)
{
    if (s_hi2c == NULL || value == NULL) return INA228_ERR_PARAM;

    uint8_t buf[2];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(
        s_hi2c,
        INA228_I2C_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        buf,
        2U,
        INA228_I2C_TIMEOUT_MS);

    if (st == HAL_TIMEOUT) return INA228_ERR_TIMEOUT;
    if (st != HAL_OK)      return INA228_ERR_I2C;

    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return INA228_OK;
}

/* Read a 24-bit register (VSHUNT, VBUS, CURRENT, POWER).
 * Returns the raw 24-bit value, right-justified in a uint32_t. */
static INA228_Status ina228_read24(uint8_t reg, uint32_t *value)
{
    if (s_hi2c == NULL || value == NULL) return INA228_ERR_PARAM;

    uint8_t buf[3];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(
        s_hi2c,
        INA228_I2C_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        buf,
        3U,
        INA228_I2C_TIMEOUT_MS);

    if (st == HAL_TIMEOUT) return INA228_ERR_TIMEOUT;
    if (st != HAL_OK)      return INA228_ERR_I2C;

    *value = ((uint32_t)buf[0] << 16) |
             ((uint32_t)buf[1] << 8)  |
             ((uint32_t)buf[2]);
    return INA228_OK;
}

/* Read a 40-bit register (ENERGY, CHARGE).
 * Returns the raw 40-bit value, right-justified in a uint64_t. */
static INA228_Status ina228_read40(uint8_t reg, uint64_t *value)
{
    if (s_hi2c == NULL || value == NULL) return INA228_ERR_PARAM;

    uint8_t buf[5];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(
        s_hi2c,
        INA228_I2C_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        buf,
        5U,
        INA228_I2C_TIMEOUT_MS);

    if (st == HAL_TIMEOUT) return INA228_ERR_TIMEOUT;
    if (st != HAL_OK)      return INA228_ERR_I2C;

    *value = ((uint64_t)buf[0] << 32) |
             ((uint64_t)buf[1] << 24) |
             ((uint64_t)buf[2] << 16) |
             ((uint64_t)buf[3] << 8)  |
             ((uint64_t)buf[4]);
    return INA228_OK;
}

/* ========================================================================== */
/*                         CONVERSION HELPERS                                  */
/* ========================================================================== */

/* VSHUNT and CURRENT registers: 24-bit raw, bits [23:4] are signed 20-bit data,
 * bits [3:0] are reserved (always 0). Sign-extend the 20-bit value. */
static int32_t signed20_from_24(uint32_t raw24)
{
    /* Drop the 4 reserved LSBs */
    int32_t v = (int32_t)(raw24 >> 4);
    /* Sign-extend from bit 19 */
    if (v & 0x00080000) {
        v |= 0xFFF00000;
    }
    return v;
}

/* VBUS register: 24-bit raw, bits [23:4] are unsigned 20-bit (always positive).
 * Bits [3:0] reserved. */
static uint32_t unsigned20_from_24(uint32_t raw24)
{
    return (raw24 >> 4) & 0x000FFFFF;
}

/* CHARGE register: 40-bit two's complement.
 * Sign-extend into int64_t. */
static int64_t signed40_from_64(uint64_t raw40)
{
    int64_t v = (int64_t)raw40;
    if (v & 0x0000008000000000LL) {
        v |= 0xFFFFFF0000000000LL;
    }
    return v;
}

/* ========================================================================== */
/*                            PUBLIC API                                       */
/* ========================================================================== */

INA228_Status INA228_Init(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) return INA228_ERR_PARAM;
    s_hi2c = hi2c;

    /* 1) Soft reset — all registers to default */
    INA228_Status st = ina228_write16(INA228_REG_CONFIG, INA228_CONFIG_RST);
    if (st != INA228_OK) return st;

    /* RST bit self-clears, but give the chip a moment */
    HAL_Delay(2);

    /* 2) Verify the chip is responding */
    st = INA228_CheckID();
    if (st != INA228_OK) return st;

    /* 3) Configure CONFIG register: ADCRANGE = 1 (±40.96 mV), no temp comp */
    st = ina228_write16(INA228_REG_CONFIG, INA228_CONFIG_ADCRANGE_40MV);
    if (st != INA228_OK) return st;

    /* 4) Configure ADC_CONFIG: continuous all, 1052µs, AVG 16 */
    st = ina228_write16(INA228_REG_ADC_CONFIG, INA228_ADC_CONFIG_DEFAULT);
    if (st != INA228_OK) return st;

    /* 5) Program shunt calibration (precomputed at compile time) */
    st = ina228_write16(INA228_REG_SHUNT_CAL, INA228_SHUNT_CAL_VALUE);
    if (st != INA228_OK) return st;

    return INA228_OK;
}

INA228_Status INA228_CheckID(void)
{
    uint16_t man_id = 0, dev_id = 0;

    INA228_Status st = ina228_read16(INA228_REG_MANUFACTURER_ID, &man_id);
    if (st != INA228_OK) return st;
    if (man_id != INA228_MANUFACTURER_ID_VAL) return INA228_ERR_ID;

    st = ina228_read16(INA228_REG_DEVICE_ID, &dev_id);
    if (st != INA228_OK) return st;
    if ((dev_id & INA228_DEVICE_ID_MASK) != INA228_DEVICE_ID_VAL) return INA228_ERR_ID;

    return INA228_OK;
}

INA228_Status INA228_Reset(void)
{
    return ina228_write16(INA228_REG_CONFIG, INA228_CONFIG_RST);
}

INA228_Status INA228_ResetAccumulators(void)
{
    /* Set RSTACC bit; keep ADCRANGE=1 to avoid disturbing config */
    return ina228_write16(INA228_REG_CONFIG,
                          INA228_CONFIG_RSTACC | INA228_CONFIG_ADCRANGE_40MV);
}

/* ----- Aggregated read --------------------------------------------------- */

INA228_Status INA228_ReadAll(INA228_Data *data)
{
    if (data == NULL) return INA228_ERR_PARAM;

    INA228_Status st;

    st = INA228_ReadBusVoltage(&data->bus_voltage_V);
    if (st != INA228_OK) return st;

    st = INA228_ReadShuntVoltage(&data->shunt_voltage_mV);
    if (st != INA228_OK) return st;

    st = INA228_ReadCurrent(&data->current_A);
    if (st != INA228_OK) return st;

    st = INA228_ReadPower(&data->power_W);
    if (st != INA228_OK) return st;

    st = INA228_ReadDieTemp(&data->die_temp_C);
    if (st != INA228_OK) return st;

    st = INA228_ReadCharge(&data->charge_C);
    if (st != INA228_OK) return st;

    st = INA228_ReadEnergy(&data->energy_J);
    if (st != INA228_OK) return st;

    return INA228_OK;
}

/* ----- Instantaneous measurements --------------------------------------- */

INA228_Status INA228_ReadBusVoltage(float *voltage_V)
{
    if (voltage_V == NULL) return INA228_ERR_PARAM;

    uint32_t raw24;
    INA228_Status st = ina228_read24(INA228_REG_VBUS, &raw24);
    if (st != INA228_OK) return st;

    uint32_t raw20 = unsigned20_from_24(raw24);
    *voltage_V = (float)raw20 * INA228_VBUS_LSB_V;
    return INA228_OK;
}

INA228_Status INA228_ReadShuntVoltage(float *voltage_mV)
{
    if (voltage_mV == NULL) return INA228_ERR_PARAM;

    uint32_t raw24;
    INA228_Status st = ina228_read24(INA228_REG_VSHUNT, &raw24);
    if (st != INA228_OK) return st;

    int32_t raw20 = signed20_from_24(raw24);
    *voltage_mV = (float)raw20 * INA228_VSHUNT_LSB_V * 1000.0f;
    return INA228_OK;
}

INA228_Status INA228_ReadCurrent(float *current_A)
{
    if (current_A == NULL) return INA228_ERR_PARAM;

    uint32_t raw24;
    INA228_Status st = ina228_read24(INA228_REG_CURRENT, &raw24);
    if (st != INA228_OK) return st;

    int32_t raw20 = signed20_from_24(raw24);
    *current_A = (float)raw20 * INA228_CURRENT_LSB_A;
    return INA228_OK;
}

INA228_Status INA228_ReadPower(float *power_W)
{
    if (power_W == NULL) return INA228_ERR_PARAM;

    uint32_t raw24;
    INA228_Status st = ina228_read24(INA228_REG_POWER, &raw24);
    if (st != INA228_OK) return st;

    /* POWER is 24-bit unsigned, all bits used */
    *power_W = (float)raw24 * INA228_POWER_LSB_W;
    return INA228_OK;
}

INA228_Status INA228_ReadDieTemp(float *temp_C)
{
    if (temp_C == NULL) return INA228_ERR_PARAM;

    uint16_t raw;
    INA228_Status st = ina228_read16(INA228_REG_DIETEMP, &raw);
    if (st != INA228_OK) return st;

    /* 16-bit two's complement */
    int16_t signed_raw = (int16_t)raw;
    *temp_C = (float)signed_raw * INA228_DIETEMP_LSB_C;
    return INA228_OK;
}

/* ----- Accumulated values ----------------------------------------------- */

INA228_Status INA228_ReadEnergy(double *energy_J)
{
    if (energy_J == NULL) return INA228_ERR_PARAM;

    uint64_t raw40;
    INA228_Status st = ina228_read40(INA228_REG_ENERGY, &raw40);
    if (st != INA228_OK) return st;

    /* ENERGY is 40-bit unsigned */
    *energy_J = (double)raw40 * (double)INA228_ENERGY_LSB_J;
    return INA228_OK;
}

INA228_Status INA228_ReadCharge(double *charge_C)
{
    if (charge_C == NULL) return INA228_ERR_PARAM;

    uint64_t raw40;
    INA228_Status st = ina228_read40(INA228_REG_CHARGE, &raw40);
    if (st != INA228_OK) return st;

    /* CHARGE is 40-bit two's complement (can be negative on discharge) */
    int64_t signed_raw = signed40_from_64(raw40);
    *charge_C = (double)signed_raw * (double)INA228_CHARGE_LSB_C;
    return INA228_OK;
}

/* ----- Raw register access ---------------------------------------------- */

INA228_Status INA228_ReadRawShunt(int32_t *raw)
{
    if (raw == NULL) return INA228_ERR_PARAM;
    uint32_t r;
    INA228_Status st = ina228_read24(INA228_REG_VSHUNT, &r);
    if (st != INA228_OK) return st;
    *raw = signed20_from_24(r);
    return INA228_OK;
}

INA228_Status INA228_ReadRawBus(uint32_t *raw)
{
    if (raw == NULL) return INA228_ERR_PARAM;
    uint32_t r;
    INA228_Status st = ina228_read24(INA228_REG_VBUS, &r);
    if (st != INA228_OK) return st;
    *raw = unsigned20_from_24(r);
    return INA228_OK;
}

INA228_Status INA228_ReadRawCurrent(int32_t *raw)
{
    if (raw == NULL) return INA228_ERR_PARAM;
    uint32_t r;
    INA228_Status st = ina228_read24(INA228_REG_CURRENT, &r);
    if (st != INA228_OK) return st;
    *raw = signed20_from_24(r);
    return INA228_OK;
}

INA228_Status INA228_ReadRawPower(uint32_t *raw)
{
    if (raw == NULL) return INA228_ERR_PARAM;
    return ina228_read24(INA228_REG_POWER, raw);
}
