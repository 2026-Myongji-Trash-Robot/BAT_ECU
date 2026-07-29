/**
 ******************************************************************************
 * @file    ina228.h
 * @brief   INA228 85-V 20-bit Power/Energy/Charge Monitor Driver
 * @author  Battery ECU Team / Capstone Design
 * @target  STM32G473CE (HAL Library)
 *
 * Configuration:
 *   - I2C Address     : 0x40  (A0=GND, A1=GND)
 *   - R_SHUNT         : 2 mΩ
 *   - I_MAX           : 15 A
 *   - ADCRANGE        : 1  (±40.96 mV, 78.125 nV/LSB)
 *   - CURRENT_LSB     : 30 µA/LSB
 *   - SHUNT_CAL       : 3146 (0xC4A)
 *
 * Reference:
 *   TI SLYS021A – INA228 Datasheet (Jan 2021, Rev. May 2022)
 ******************************************************************************
 */

#ifndef __INA228_H
#define __INA228_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* ========================================================================== */
/*                           USER CONFIGURATION                                */
/* ========================================================================== */
#define INA228_I2C_ADDR_7BIT     (0x40)         /* A0=GND, A1=GND */
#define INA228_I2C_ADDR          (INA228_I2C_ADDR_7BIT << 1)  /* HAL uses 8-bit */

#define INA228_I2C_TIMEOUT_MS    (100U)

/* Hardware parameters */
#define INA228_R_SHUNT_OHM       (0.002f)       /* 2 mΩ */
#define INA228_I_MAX_A           (15.0f)        /* 15 A */

/* Pre-calculated calibration values
 * CURRENT_LSB = 15A / 2^19 ≈ 28.6 µA  → round to 30 µA for clean math
 * SHUNT_CAL = 13107.2e6 × CURRENT_LSB × R_SHUNT × 4   (×4 for ADCRANGE=1)
 *           = 13107.2e6 × 30e-6 × 2e-3 × 4
 *           ≈ 3146
 */
#define INA228_CURRENT_LSB_A     (30.0e-6f)     /* 30 µA per LSB */
#define INA228_SHUNT_CAL_VALUE   (3146U)        /* 0xC4A */

/* ========================================================================== */
/*                           REGISTER MAP                                      */
/* ========================================================================== */
#define INA228_REG_CONFIG            0x00U  /* 16-bit  R/W */
#define INA228_REG_ADC_CONFIG        0x01U  /* 16-bit  R/W */
#define INA228_REG_SHUNT_CAL         0x02U  /* 16-bit  R/W */
#define INA228_REG_SHUNT_TEMPCO      0x03U  /* 16-bit  R/W */
#define INA228_REG_VSHUNT            0x04U  /* 24-bit  R  */
#define INA228_REG_VBUS              0x05U  /* 24-bit  R  */
#define INA228_REG_DIETEMP           0x06U  /* 16-bit  R  */
#define INA228_REG_CURRENT           0x07U  /* 24-bit  R  */
#define INA228_REG_POWER             0x08U  /* 24-bit  R  */
#define INA228_REG_ENERGY            0x09U  /* 40-bit  R  */
#define INA228_REG_CHARGE            0x0AU  /* 40-bit  R  */
#define INA228_REG_DIAG_ALRT         0x0BU  /* 16-bit  R/W */
#define INA228_REG_SOVL              0x0CU  /* 16-bit  R/W */
#define INA228_REG_SUVL              0x0DU  /* 16-bit  R/W */
#define INA228_REG_BOVL              0x0EU  /* 16-bit  R/W */
#define INA228_REG_BUVL              0x0FU  /* 16-bit  R/W */
#define INA228_REG_TEMP_LIMIT        0x10U  /* 16-bit  R/W */
#define INA228_REG_PWR_LIMIT         0x11U  /* 16-bit  R/W */
#define INA228_REG_MANUFACTURER_ID   0x3EU  /* 16-bit  R   = 0x5449 ("TI") */
#define INA228_REG_DEVICE_ID         0x3FU  /* 16-bit  R   = 0x2281 */

/* Expected ID values */
#define INA228_MANUFACTURER_ID_VAL   0x5449U  /* "TI" in ASCII */
#define INA228_DEVICE_ID_MASK        0xFFF0U
#define INA228_DEVICE_ID_VAL         0x2280U  /* Upper 12 bits */

/* ========================================================================== */
/*                       CONFIG REGISTER BIT FIELDS                            */
/* ========================================================================== */
#define INA228_CONFIG_RST            (1U << 15) /* System reset */
#define INA228_CONFIG_RSTACC         (1U << 14) /* Reset ENERGY/CHARGE */
#define INA228_CONFIG_TEMPCOMP       (1U << 5)  /* Shunt temp compensation */
#define INA228_CONFIG_ADCRANGE_40MV  (1U << 4)  /* 1 = ±40.96mV, 0 = ±163.84mV */

/* ========================================================================== */
/*                     ADC_CONFIG REGISTER BIT FIELDS                          */
/* ========================================================================== */
/* MODE (bits 15-12) — continuous shunt/bus/temp = 0xF */
#define INA228_MODE_SHUTDOWN          (0x0U << 12)
#define INA228_MODE_TRIG_BUS          (0x1U << 12)
#define INA228_MODE_TRIG_SHUNT        (0x2U << 12)
#define INA228_MODE_TRIG_SHUNT_BUS    (0x3U << 12)
#define INA228_MODE_TRIG_TEMP         (0x4U << 12)
#define INA228_MODE_TRIG_TEMP_BUS     (0x5U << 12)
#define INA228_MODE_TRIG_TEMP_SHUNT   (0x6U << 12)
#define INA228_MODE_TRIG_ALL          (0x7U << 12)
#define INA228_MODE_CONT_BUS          (0x9U << 12)
#define INA228_MODE_CONT_SHUNT        (0xAU << 12)
#define INA228_MODE_CONT_SHUNT_BUS    (0xBU << 12)
#define INA228_MODE_CONT_TEMP         (0xCU << 12)
#define INA228_MODE_CONT_TEMP_BUS     (0xDU << 12)
#define INA228_MODE_CONT_TEMP_SHUNT   (0xEU << 12)
#define INA228_MODE_CONT_ALL          (0xFU << 12)

/* Conversion time (VBUSCT 11-9, VSHCT 8-6, VTCT 5-3) */
#define INA228_CT_50US        0x0U
#define INA228_CT_84US        0x1U
#define INA228_CT_150US       0x2U
#define INA228_CT_280US       0x3U
#define INA228_CT_540US       0x4U
#define INA228_CT_1052US      0x5U
#define INA228_CT_2074US      0x6U
#define INA228_CT_4120US      0x7U

/* Averaging count (AVG 2-0) */
#define INA228_AVG_1          0x0U
#define INA228_AVG_4          0x1U
#define INA228_AVG_16         0x2U
#define INA228_AVG_64         0x3U
#define INA228_AVG_128        0x4U
#define INA228_AVG_256        0x5U
#define INA228_AVG_512        0x6U
#define INA228_AVG_1024       0x7U

/* Helper to build ADC_CONFIG value */
#define INA228_ADC_CONFIG_VAL(mode, vbusct, vshct, vtct, avg) \
    ((uint16_t)((mode) | ((vbusct) << 9) | ((vshct) << 6) | ((vtct) << 3) | (avg)))

/* Default: continuous all + 1052µs + AVG 16 — good balance for 10ms CAN cycle */
#define INA228_ADC_CONFIG_DEFAULT \
    INA228_ADC_CONFIG_VAL(INA228_MODE_CONT_ALL, \
                          INA228_CT_1052US, \
                          INA228_CT_1052US, \
                          INA228_CT_1052US, \
                          INA228_AVG_16)

/* ========================================================================== */
/*                  CONVERSION FACTORS (datasheet Table 8-1)                   */
/* ========================================================================== */
#define INA228_VSHUNT_LSB_V          (78.125e-9f)    /* ADCRANGE=1: 78.125 nV */
#define INA228_VBUS_LSB_V            (195.3125e-6f)  /* 195.3125 µV */
#define INA228_DIETEMP_LSB_C         (7.8125e-3f)    /* 7.8125 m°C */

/* POWER_LSB = 3.2 × CURRENT_LSB (datasheet Eq. 4) */
#define INA228_POWER_LSB_W           (3.2f * INA228_CURRENT_LSB_A)
/* ENERGY_LSB = 16 × POWER_LSB (datasheet Eq. 5) */
#define INA228_ENERGY_LSB_J          (16.0f * INA228_POWER_LSB_W)
/* CHARGE_LSB = CURRENT_LSB × 1s (datasheet Eq. 6) */
#define INA228_CHARGE_LSB_C          (INA228_CURRENT_LSB_A)

/* ========================================================================== */
/*                              TYPES                                          */
/* ========================================================================== */
typedef enum {
    INA228_OK       = 0,
    INA228_ERR_I2C,
    INA228_ERR_TIMEOUT,
    INA228_ERR_PARAM,
    INA228_ERR_ID            /* Manufacturer / Device ID mismatch */
} INA228_Status;

/**
 * @brief  Aggregated measurement snapshot read in one polling cycle.
 *         All fields are in physical (SI) units, already converted from
 *         raw register values using the datasheet conversion factors.
 *
 *         Use INA228_ReadAll() to populate this struct in one shot.
 */
typedef struct {
    float  bus_voltage_V;     /* VBUS  : 0 V to 85 V                      */
    float  shunt_voltage_mV;  /* VSHUNT: ±40.96 mV (signed)                */
    float  current_A;         /* CURRENT: ±I_MAX (signed)                  */
    float  power_W;           /* POWER : always positive                   */
    float  die_temp_C;        /* DIETEMP: -40 .. +125 °C                   */
    double charge_C;          /* CHARGE: Coulombs (signed, 40-bit)         */
    double energy_J;          /* ENERGY: Joules (unsigned, 40-bit)         */
} INA228_Data;

/* ========================================================================== */
/*                              PUBLIC API                                     */
/* ========================================================================== */

/**
 * @brief  Initialize INA228: register I2C handle, soft-reset, configure ADC,
 *         and program shunt calibration.
 * @param  hi2c  Pointer to STM32 HAL I2C handle (e.g. &hi2c1)
 * @retval INA228_Status
 */
INA228_Status INA228_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Verify device by reading MANUFACTURER_ID and DEVICE_ID.
 *         Call this after Init to confirm the chip is alive.
 */
INA228_Status INA228_CheckID(void);

/**
 * @brief  Software reset (sets all registers to default; re-init required).
 */
INA228_Status INA228_Reset(void);

/* --- Aggregated read (recommended for periodic polling) ----------------- */
/**
 * @brief  Read **all** measurement registers in one call and fill an
 *         INA228_Data struct. This is the recommended API for the main
 *         polling loop — the caller declares one struct instead of many
 *         individual variables.
 *
 *         If any sub-read fails, the function returns the corresponding
 *         error code and stops at the failing field (later fields may be
 *         left untouched).
 *
 * @param  data  Pointer to caller-allocated INA228_Data
 * @retval INA228_OK on success, error code on first failure.
 */
INA228_Status INA228_ReadAll(INA228_Data *data);

/* --- Instantaneous measurements (individual access) --------------------- */
INA228_Status INA228_ReadBusVoltage(float *voltage_V);
INA228_Status INA228_ReadShuntVoltage(float *voltage_mV);
INA228_Status INA228_ReadCurrent(float *current_A);
INA228_Status INA228_ReadPower(float *power_W);
INA228_Status INA228_ReadDieTemp(float *temp_C);

/* --- Accumulated values (for SoC layer) --------------------------------- */
/**
 * @brief  Energy accumulator (40-bit unsigned, in Joules).
 *         NOTE: Continuous mode required (triggered mode invalidates this).
 */
INA228_Status INA228_ReadEnergy(double *energy_J);

/**
 * @brief  Charge accumulator (40-bit two's complement, in Coulombs).
 *         Use this for Coulomb counting in SoC estimation.
 */
INA228_Status INA228_ReadCharge(double *charge_C);

/**
 * @brief  Reset only ENERGY and CHARGE accumulators (CONFIG.RSTACC).
 *         Use this when re-initializing SoC tracking.
 */
INA228_Status INA228_ResetAccumulators(void);

/* --- Raw register access (debug / logging) ------------------------------ */
INA228_Status INA228_ReadRawShunt(int32_t  *raw);   /* 20-bit signed */
INA228_Status INA228_ReadRawBus(uint32_t  *raw);    /* 20-bit unsigned */
INA228_Status INA228_ReadRawCurrent(int32_t *raw);  /* 20-bit signed */
INA228_Status INA228_ReadRawPower(uint32_t *raw);   /* 24-bit unsigned */

#ifdef __cplusplus
}
#endif

#endif /* __INA228_H */
