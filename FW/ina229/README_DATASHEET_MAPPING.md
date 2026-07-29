# INA228 드라이버 — 데이터시트 근거 매핑 문서

> **Reference Datasheet**: TI **SLYS021A** — *INA228 85-V, 20-Bit, Ultra-Precise Power/Energy/Charge Monitor With I²C Interface* (January 2021, Revised May 2022)
>
> 이 문서는 `ina228.h` / `ina228.c`의 **모든 매크로, 함수, 비트 연산, 변환식**이 데이터시트의 어느 섹션/표/식/페이지에 근거하는지 한 줄도 빠짐없이 매핑합니다.
>
> 페이지 번호와 섹션 번호는 모두 SLYS021A 기준입니다.

---

## 목차
1. [상위 설계 결정의 근거](#1-상위-설계-결정의-근거)
2. [USER CONFIGURATION 매크로 근거](#2-user-configuration-매크로-근거)
3. [레지스터 맵 매크로 근거](#3-레지스터-맵-매크로-근거)
4. [CONFIG 레지스터 비트필드 근거](#4-config-레지스터-비트필드-근거)
5. [ADC_CONFIG 레지스터 비트필드 근거](#5-adc_config-레지스터-비트필드-근거)
6. [변환 계수(LSB) 매크로 근거](#6-변환-계수lsb-매크로-근거)
7. [I²C 통신 헬퍼 함수 근거](#7-ic-통신-헬퍼-함수-근거)
8. [부호확장 헬퍼 함수 근거](#8-부호확장-헬퍼-함수-근거)
9. [INA228_Init() 시퀀스 근거](#9-ina228_init-시퀀스-근거)
10. [INA228_CheckID() 근거](#10-ina228_checkid-근거)
11. [측정값 읽기 함수 근거 (V/I/P/T)](#11-측정값-읽기-함수-근거-vipt)
12. [누적값 읽기 함수 근거 (Energy/Charge)](#12-누적값-읽기-함수-근거-energycharge)
13. [INA228_Data 구조체와 INA228_ReadAll() 근거](#13-ina228_data-구조체와-ina228_readall-근거)
14. [전체 신호 흐름과 평균/누적의 의미](#14-전체-신호-흐름과-평균누적의-의미)

---

## 1. 상위 설계 결정의 근거

### 1.1 단일 디바이스, HAL 직결 C
회로도(`BAT_ECU_PCB_V1`) 상에서 INA228은 **1개만** 사용되며 STM32G473CE의 I²C1에 연결되어 있음. 멀티 인스턴스가 불필요하므로 전역 static 핸들 `s_hi2c`만 보관하는 가장 단순한 구조 채택.

### 1.2 폴링 방식 채택
회로도상 INA228의 `~ALERT` 핀이 MCU에 연결되어 있지만, BMS의 정상 동작 주기(보통 10ms)는 인터럽트 없이도 충분히 응답 가능. 폴링은 펌웨어 디버깅이 가장 단순하고 CAN 전송 주기와 동기화가 쉬움.

### 1.3 ADCRANGE = 1 (±40.96 mV) 채택
- 데이터시트 §8.1.1, Table 8-1 (p.30)
- 최대 션트 전압 = I_MAX × R_SHUNT = 15A × 2mΩ = **30 mV** < 40.96 mV ✓
- ±40.96 mV 모드의 LSB = **78.125 nV** (vs ±163.84 mV 모드의 312.5 nV/LSB → **4배 정밀**)
- 저전류 영역에서 분해능이 4배 향상되어 BMS의 슬립/대기 전류 측정에 유리

---

## 2. USER CONFIGURATION 매크로 근거

### 2.1 `INA228_I2C_ADDR_7BIT = 0x40`
- **근거**: 데이터시트 §7.5, **Table 7-2** (p.19), *Address Pins and Secondary Device Addresses*
- 회로도상 `A1 = GND`, `A0 = GND` → 7-bit 주소 `1000000b = 0x40`

### 2.2 `INA228_I2C_ADDR = (0x40 << 1)`
- **근거**: STM32 HAL API 규약 (데이터시트 외부)
- `HAL_I2C_Mem_Write/Read`는 7비트 주소를 1비트 좌측 시프트한 **8비트 형태**로 입력받음
- 데이터시트 Figure 7-7, 7-8에 표시된 프레임에서 R/W̄ 비트 자리에 시프트된 형태

### 2.3 `INA228_R_SHUNT_OHM = 0.002f`, `INA228_I_MAX_A = 15.0f`
- **근거**: 데이터시트 §8.2.1, **Table 8-3** (p.35) *Design Parameters*에 따른 사용자 시스템 설계 파라미터
- 데이터시트의 예시(IMAX=10A, R_SHUNT=16.2mΩ)를 우리 시스템(15A, 2mΩ)에 적용

### 2.4 `INA228_CURRENT_LSB_A = 30.0e-6f`
- **근거**: 데이터시트 **식 (3)** (p.31)
  $$\text{CURRENT\_LSB} = \frac{I_{\text{MAX}}}{2^{19}}$$
- 계산: 15 / 524288 ≈ 2.861 × 10⁻⁵ ≈ **28.6 µA/LSB**
- 데이터시트 §8.1.2 (p.30) 권고:
  > "it is common to select a higher round-number (no higher than 8x) value for the CURRENT_LSB in order to simplify the conversion of the CURRENT."
- 따라서 **30 µA/LSB**로 라운드. 이는 식 (3)의 결과보다 1.05배 큰 값으로, 8배 이내 권장 범위 안.

### 2.5 `INA228_SHUNT_CAL_VALUE = 3146` (0xC4A)
- **근거**: 데이터시트 **식 (2)** (p.31)
  $$\text{SHUNT\_CAL} = 13107.2 \times 10^6 \times \text{CURRENT\_LSB} \times R_{\text{SHUNT}}$$
- ADCRANGE = 1이면 식 (2)에 추가로 **×4** (데이터시트 §8.1.2 명시)
- 계산:
  $$13107.2 \times 10^6 \times 30 \times 10^{-6} \times 2 \times 10^{-3} \times 4 = 3145.728$$
- → 정수로 라운드하여 **3146 (0xC4A)**
- SHUNT_CAL 레지스터의 유효 비트: 14-0 (15비트), 최대값 0x7FFF = 32767 → 3146은 충분히 범위 내

---

## 3. 레지스터 맵 매크로 근거

**근거**: 데이터시트 §7.6.1, **Table 7-3** (p.21) *INA228 Registers*

| 매크로 | 값 | 데이터시트 |
|---|---|---|
| `INA228_REG_CONFIG` | 0x00 | Table 7-3, §7.6.1.1 (p.22) |
| `INA228_REG_ADC_CONFIG` | 0x01 | Table 7-3, §7.6.1.2 (p.22-23) |
| `INA228_REG_SHUNT_CAL` | 0x02 | Table 7-3, §7.6.1.3 (p.24) |
| `INA228_REG_SHUNT_TEMPCO` | 0x03 | Table 7-3, §7.6.1.4 (p.24) |
| `INA228_REG_VSHUNT` | 0x04 | Table 7-3, §7.6.1.5 (p.24-25) |
| `INA228_REG_VBUS` | 0x05 | Table 7-3, §7.6.1.6 (p.25) |
| `INA228_REG_DIETEMP` | 0x06 | Table 7-3, §7.6.1.7 (p.25) |
| `INA228_REG_CURRENT` | 0x07 | Table 7-3, §7.6.1.8 (p.25) |
| `INA228_REG_POWER` | 0x08 | Table 7-3, §7.6.1.9 (p.25-26) |
| `INA228_REG_ENERGY` | 0x09 | Table 7-3, §7.6.1.10 (p.26) |
| `INA228_REG_CHARGE` | 0x0A | Table 7-3, §7.6.1.11 (p.26) |
| `INA228_REG_DIAG_ALRT` | 0x0B | Table 7-3, §7.6.1.12 (p.26) |
| `INA228_REG_SOVL`~`PWR_LIMIT` | 0x0C~0x11 | §7.6.1.13~18 (p.27-28) |
| `INA228_REG_MANUFACTURER_ID` | 0x3E | §7.6.1.19, Table 7-23 (p.29) |
| `INA228_REG_DEVICE_ID` | 0x3F | §7.6.1.20, Table 7-24 (p.29) |

### 3.1 ID 매크로
- `INA228_MANUFACTURER_ID_VAL = 0x5449`
  - **근거**: §7.6.1.19, Table 7-23 — "Reads back **TI** in ASCII" (`T`=0x54, `I`=0x49)
- `INA228_DEVICE_ID_MASK = 0xFFF0`, `INA228_DEVICE_ID_VAL = 0x2280`
  - **근거**: §7.6.1.20, Table 7-24
    - bits [15:4] = DIEID = `228h` (고정)
    - bits [3:0] = REV_ID (리비전마다 다름)
  - 따라서 상위 12비트만 마스킹하여 비교

---

## 4. CONFIG 레지스터 비트필드 근거

**근거**: 데이터시트 §7.6.1.1, **Table 7-5** *CONFIG Register Field Descriptions* (p.22)

| 매크로 | 비트 | 의미 | 비고 |
|---|---|---|---|
| `INA228_CONFIG_RST` | bit 15 | System Reset (셀프클리어) | Table 7-5 |
| `INA228_CONFIG_RSTACC` | bit 14 | ENERGY/CHARGE 누적 레지스터 초기화 | Table 7-5 |
| `INA228_CONFIG_TEMPCOMP` | bit 5 | 외부 션트 온도보상 활성 | Table 7-5 |
| `INA228_CONFIG_ADCRANGE_40MV` | bit 4 | 1 = ±40.96 mV / 0 = ±163.84 mV | Table 7-5 |
| bit 13-6 (CONVDLY) | — | 본 라이브러리에서 0 유지 | 사용 안 함 |
| bit 3-0 RESERVED | — | 항상 0 | Table 7-5 |

---

## 5. ADC_CONFIG 레지스터 비트필드 근거

**근거**: 데이터시트 §7.6.1.2, **Table 7-6** *ADC_CONFIG Register Field Descriptions* (p.22-23)

### 5.1 MODE 비트 (bits 15-12)
Table 7-6에 명시된 16가지 모드 모두 매크로화:

| 매크로 | 코드 | 동작 |
|---|---|---|
| `INA228_MODE_SHUTDOWN` | 0h, 8h | Shutdown |
| `INA228_MODE_TRIG_BUS` | 1h | Triggered bus voltage, single shot |
| `INA228_MODE_TRIG_SHUNT` | 2h | Triggered shunt voltage, single shot |
| `INA228_MODE_TRIG_SHUNT_BUS` | 3h | Triggered shunt + bus, single shot |
| `INA228_MODE_TRIG_TEMP` | 4h | Triggered temperature, single shot |
| `INA228_MODE_TRIG_TEMP_BUS` | 5h | Triggered temp + bus |
| `INA228_MODE_TRIG_TEMP_SHUNT` | 6h | Triggered temp + shunt |
| `INA228_MODE_TRIG_ALL` | 7h | Triggered bus + shunt + temp |
| `INA228_MODE_CONT_BUS` | 9h | Continuous bus voltage only |
| `INA228_MODE_CONT_SHUNT` | Ah | Continuous shunt voltage only |
| `INA228_MODE_CONT_SHUNT_BUS` | Bh | Continuous shunt and bus |
| `INA228_MODE_CONT_TEMP` | Ch | Continuous temperature only |
| `INA228_MODE_CONT_TEMP_BUS` | Dh | Continuous bus + temp |
| `INA228_MODE_CONT_TEMP_SHUNT` | Eh | Continuous temp + shunt |
| `INA228_MODE_CONT_ALL` | **Fh** | **Continuous bus + shunt + temp** ← 본 라이브러리 기본값 |

본 라이브러리는 `0xF`(연속 모드 + 3개 채널 모두)를 채택. 이유는 데이터시트 §7.3.3 (p.14):
> *"In triggered mode, the accumulation registers (ENERGY and CHARGE) are invalid, as the device does not keep track of elapsed time."*
>
> 즉, **SoC 계산에 필요한 CHARGE 누적**을 쓰려면 **연속 모드가 필수**.

### 5.2 변환시간 매크로 (VBUSCT/VSHCT/VTCT)
- **근거**: Table 7-6, bits 11-9 / 8-6 / 5-3
- 모두 동일한 8가지 코드(0h~7h):

| 매크로 | 코드 | 시간 |
|---|---|---|
| `INA228_CT_50US` | 0h | 50 µs |
| `INA228_CT_84US` | 1h | 84 µs |
| `INA228_CT_150US` | 2h | 150 µs |
| `INA228_CT_280US` | 3h | 280 µs |
| `INA228_CT_540US` | 4h | 540 µs |
| **`INA228_CT_1052US`** | 5h | 1052 µs ← 라이브러리 기본값 |
| `INA228_CT_2074US` | 6h | 2074 µs |
| `INA228_CT_4120US` | 7h | 4120 µs |

### 5.3 AVG 매크로 (bits 2-0)
- **근거**: Table 7-6, bits 2-0 (p.24)
- 8단계 평균(1, 4, 16, 64, 128, 256, 512, 1024) 모두 매크로화

### 5.4 `INA228_ADC_CONFIG_VAL()` 매크로
**데이터시트 Table 7-6의 비트 배치를 그대로 비트 시프트한 것.**

```c
((mode) | ((vbusct) << 9) | ((vshct) << 6) | ((vtct) << 3) | (avg))
```

| 영역 | 비트 위치 | 데이터시트 |
|---|---|---|
| MODE | 15-12 (이미 시프트된 매크로) | Table 7-6 |
| VBUSCT | 11-9 → `<< 9` | Table 7-6 |
| VSHCT | 8-6 → `<< 6` | Table 7-6 |
| VTCT | 5-3 → `<< 3` | Table 7-6 |
| AVG | 2-0 → 그대로 | Table 7-6 |

### 5.5 기본값 `INA228_ADC_CONFIG_DEFAULT`
- `MODE = Fh` (Continuous all)
- `VBUSCT = VSHCT = VTCT = 5h` (1052 µs)
- `AVG = 2h` (16샘플 평균)

**근거**: 데이터시트 §8.1.3, **Table 8-2** *INA228 Noise Performance* (p.32)
- 1052 µs + 16평균 → 출력 샘플 주기 = **4.208 ms**
- ADCRANGE=1에서 Noise-Free ENOB ≈ **13.8 비트**
- 10 ms CAN 송신 주기보다 짧아 다음 송신 시점마다 최신 평균값 활용 가능

---

## 6. 변환 계수(LSB) 매크로 근거

### 6.1 `INA228_VSHUNT_LSB_V = 78.125e-9f`
- **근거**: 데이터시트 §7.6.1.5, **Table 7-9** (p.24-25)
  > *"Conversion factor: 312.5 nV/LSB when ADCRANGE = 0; **78.125 nV/LSB when ADCRANGE = 1**"*
- 또한 §8.1.1, **Table 8-1** (p.30)에 동일하게 명시

### 6.2 `INA228_VBUS_LSB_V = 195.3125e-6f`
- **근거**: 데이터시트 §7.6.1.6, **Table 7-10** (p.25)
  > *"Conversion factor: 195.3125 µV/LSB"*

### 6.3 `INA228_DIETEMP_LSB_C = 7.8125e-3f`
- **근거**: 데이터시트 §7.6.1.7, **Table 7-11** (p.25)
  > *"Conversion factor: 7.8125 m°C/LSB"*

### 6.4 `INA228_POWER_LSB_W = 3.2f * CURRENT_LSB`
- **근거**: 데이터시트 **식 (5)** (p.31)
  $$\text{Power [W]} = 3.2 \times \text{CURRENT\_LSB} \times \text{POWER}$$
- 즉 POWER 레지스터 1 LSB의 물리량 = 3.2 × CURRENT_LSB

### 6.5 `INA228_ENERGY_LSB_J = 16.0f * POWER_LSB`
- **근거**: 데이터시트 **식 (6)** (p.31)
  $$\text{Energy [J]} = 16 \times 3.2 \times \text{CURRENT\_LSB} \times \text{ENERGY}$$
- 따라서 ENERGY 1 LSB = 16 × 3.2 × CURRENT_LSB = 16 × POWER_LSB

### 6.6 `INA228_CHARGE_LSB_C = CURRENT_LSB`
- **근거**: 데이터시트 **식 (7)** (p.32)
  $$\text{Charge [C]} = \text{CURRENT\_LSB} \times \text{CHARGE}$$
- CHARGE 1 LSB = CURRENT_LSB (× 1초, 단위가 쿨롱이지만 LSB 자체는 동일)

---

## 7. I²C 통신 헬퍼 함수 근거

### 7.1 `ina228_write16()` / `ina228_read16()`
- **근거**: 데이터시트 §7.5.1.1 *Writing to and Reading Through the I2C Serial Interface* (p.19)
  - **Figure 7-7** *Timing Diagram for Write Word Format*
  - **Figure 7-8** *Timing Diagram for Read Word Format*
- 핵심 사실: "Register bytes are sent **most-significant byte first**, followed by the least significant byte."
  - 따라서 16비트 쓰기에서 `buf[0] = MSB`, `buf[1] = LSB`
  - 16비트 읽기에서 `value = (buf[0] << 8) | buf[1]`

### 7.2 `ina228_read24()` (24비트 레지스터)
- **근거**: 데이터시트 §7.5.1.1 (p.20)
  > *"These diagrams are shown for reading/writing to 16 bit registers. Registers with a higher number of bytes will behave similarly."*
- 즉 24비트 레지스터(VSHUNT, VBUS, CURRENT, POWER)도 동일한 프로토콜로 3바이트 연속 read
- MSB first 규칙 그대로 적용: `value = (buf[0] << 16) | (buf[1] << 8) | buf[2]`

### 7.3 `ina228_read40()` (40비트 레지스터)
- **근거**: 동일하게 §7.5.1.1
- ENERGY/CHARGE는 40비트 → 5바이트 연속 read
- MSB first: `value = (buf[0] << 32) | (buf[1] << 24) | ... | buf[4]`

### 7.4 STM32 HAL_I2C_Mem_*  API 사용 이유
- INA228의 I²C 프로토콜은 "주소 + 레지스터 포인터 + 데이터" 형태(§7.5.1.1)인데, 이게 정확히 STM32 HAL의 `HAL_I2C_Mem_Read/Write`의 동작 그대로
- 즉, `MemAddress` 파라미터가 INA228의 register pointer 역할
- `I2C_MEMADD_SIZE_8BIT` 사용 이유: 레지스터 포인터가 1바이트(데이터시트 Figure 7-7 *Frame 2 Register Pointer Byte*)

---

## 8. 부호확장 헬퍼 함수 근거

### 8.1 `signed20_from_24()`
- **근거**: 데이터시트 §7.6.1.5 **Table 7-9** (VSHUNT), §7.6.1.8 **Table 7-12** (CURRENT)
  - bits 23-4 = 실제 측정값 (20-bit, **Two's complement**)
  - bits 3-0 = RESERVED, 항상 0
- 구현 단계:
  1. `raw24 >> 4` → 하위 4비트(RESERVED) 제거하여 20비트 값 얻음
  2. bit 19 (sign bit) 확인: `v & 0x00080000`
  3. 음수면 상위 12비트(`0xFFF00000`)를 1로 채워 int32_t 부호확장

### 8.2 `unsigned20_from_24()` (VBUS 전용)
- **근거**: 데이터시트 §7.6.1.6, **Table 7-10** (p.25)
  > *"Bus voltage output. Two's complement value, **however always positive**."*
- VBUS는 2의 보수로 저장되지만 항상 양수이므로 부호확장 불필요
- `(raw24 >> 4) & 0x000FFFFF`로 20비트만 추출

### 8.3 `signed40_from_64()`
- **근거**: 데이터시트 §7.6.1.11, **Table 7-15** (p.26)
  > *"Calculated charge output. Output value is in Coulombs. **Two's complement value**."*
- CHARGE는 음수 가능(방전 시) → 부호확장 필수
- bit 39 (sign bit) = `0x0000008000000000LL`
- 음수면 상위 24비트(`0xFFFFFF0000000000LL`)를 1로 채움

### 8.4 ENERGY는 부호확장 안 함
- **근거**: §7.6.1.10, **Table 7-14** (p.26)
  > *"Output value is in Joules. **Unsigned representation. Positive value.**"*
- 따라서 ENERGY는 raw 40비트를 그대로 uint64_t로 사용

### 8.5 DIETEMP는 16비트 그대로 두 보수
- **근거**: §7.6.1.7, **Table 7-11** (p.25)
  > *"Internal die temperature measurement. **Two's complement value**."*
- C 표준에서 `int16_t`로 캐스팅하면 자동 부호확장 → 별도 헬퍼 불필요

---

## 9. INA228_Init() 시퀀스 근거

이 함수의 5단계 순서는 데이터시트 §8.2.2 *Detailed Design Procedure* (p.35-36)를 그대로 따름.

### Step 1 — Soft Reset (`CONFIG.RST = 1`)
- **근거**: §7.6.1.1, Table 7-5 (p.22)
  > *"Setting this bit to '1' generates a system reset that is the same as power-on reset. Resets all registers to default values. This bit self-clears."*
- 보드 리셋 없이 모듈만 깨끗한 상태로 두기 위함
- `HAL_Delay(2)`: 셀프클리어 + 내부 안정화 마진

### Step 2 — ID 확인 (`INA228_CheckID()`)
- **근거**: §7.6.1.19 (MANUFACTURER_ID, p.29), §7.6.1.20 (DEVICE_ID, p.29)
- 데이터시트 §8.2.2에는 명시되지 않지만, 실무상 I²C 통신 OK 확인 + 잘못된 칩 식별을 위한 표준 절차
- MANUFACTURER_ID = 0x5449("TI") 검증 → 가장 신뢰성 있는 첫 통신 확인 방법

### Step 3 — CONFIG 설정 (ADCRANGE = 1)
- **근거**: §8.2.2.2 *Configure the Device* (p.35)
  > *"If the default power up conditions do not meet the design requirements, these registers will need to be set properly after each VS power cycle event."*
- 우리 시스템(R_SHUNT=2mΩ, I_MAX=15A → V_sense_max=30mV)은 ±40.96mV 모드에 적합
- `INA228_CONFIG_ADCRANGE_40MV`만 set, 나머지 bit는 0 (TEMPCOMP off, CONVDLY=0)

### Step 4 — ADC_CONFIG 설정
- **근거**: §8.2.2.2 (p.35) + Table 7-6 (p.22-23)
- §7.3.3 (p.14) 근거로 연속 모드(MODE=Fh) 필수
- §8.1.3 Table 8-2 (p.32) 근거로 1052µs + AVG 16 채택

### Step 5 — SHUNT_CAL 프로그래밍
- **근거**: §8.2.2.3 *Program the Shunt Calibration Register* (p.35)
  > *"Failure to set the value of the shunt calibration register will result in a zero value for any result based on current."*
- §7.3.2 (p.13)도 동일하게 강조:
  > *"If the value loaded into the SHUNT_CAL register is zero, the power, energy and charge values will be reported as zero."*
- 따라서 SHUNT_CAL은 **반드시** 마지막 단계에서 설정해야 CURRENT/POWER/ENERGY/CHARGE 모두 활성화

### CONFIG 쓰기 순서 주의
데이터시트 §8.1.2 (p.31) 강조:
> *"For ADCRANGE = 1, the value of SHUNT_CAL must be multiplied by 4."*

즉, ADCRANGE는 SHUNT_CAL **이전에** 설정되어 있어야 함. 본 라이브러리는 Step 3에서 ADCRANGE를 먼저 set한 뒤 Step 5에서 SHUNT_CAL을 쓰므로 순서 위배 없음.

---

## 10. INA228_CheckID() 근거

### MANUFACTURER_ID 검증
- **근거**: §7.6.1.19, **Table 7-23** (p.29)
  > *"Reads back TI in ASCII."*
- T=0x54, I=0x49 → 0x5449
- 이 값이 다르면 I²C 자체가 실패했거나 다른 칩이 응답한 것

### DEVICE_ID 검증 (마스킹)
- **근거**: §7.6.1.20, **Table 7-24** (p.29)
  - bits [15:4] = DIEID = 228h
  - bits [3:0] = REV_ID
- REV_ID는 제조 리비전마다 다를 수 있으므로 `& 0xFFF0` 마스킹 후 0x2280 비교
- 데이터시트 reset value는 0x2281이지만 REV_ID 변동 가능성 대비

---

## 11. 측정값 읽기 함수 근거 (V/I/P/T)

### 11.1 `INA228_ReadBusVoltage()`
**근거**: §7.6.1.6 Table 7-10 (p.25) + §8.1.1 Table 8-1 (p.30)

```c
raw20 = unsigned20_from_24(raw24);  // bits 23-4 → 20-bit unsigned
*voltage_V = raw20 * 195.3125e-6f;   // VBUS LSB = 195.3125 µV
```

- VBUS는 항상 양수 (§7.6.1.6 "always positive")
- Full-scale 범위: 0 ~ 85 V (Table 8-1)

### 11.2 `INA228_ReadShuntVoltage()`
**근거**: §7.6.1.5 Table 7-9 (p.24-25)

```c
raw20 = signed20_from_24(raw24);     // bits 23-4, 부호확장
*voltage_mV = raw20 * 78.125e-9f * 1000.0f;  // nV → mV
```

- ADCRANGE=1이므로 LSB = **78.125 nV**
- 부호확장 처리 — 양방향 전류(charge/discharge) 모두 측정 가능

### 11.3 `INA228_ReadCurrent()`
**근거**: §7.6.1.8 Table 7-12 (p.25) + **식 (4)** (p.31)

$$\text{Current [A]} = \text{CURRENT\_LSB} \times \text{CURRENT register}$$

```c
raw20 = signed20_from_24(raw24);
*current_A = raw20 * INA228_CURRENT_LSB_A;  // 30 µA/LSB
```

- CURRENT 레지스터는 INA228이 내부적으로 `VSHUNT × SHUNT_CAL` 연산을 거쳐 만든 값 (§7.3.2 Internal Measurement and Calculation Engine, p.13)
- 따라서 SHUNT_CAL이 0이면 항상 0 반환 — `Init()`에서 SHUNT_CAL 프로그래밍이 필수인 이유

### 11.4 `INA228_ReadPower()`
**근거**: §7.6.1.9 Table 7-13 (p.25-26) + **식 (5)** (p.31)

$$\text{Power [W]} = 3.2 \times \text{CURRENT\_LSB} \times \text{POWER register}$$

```c
*power_W = raw24 * INA228_POWER_LSB_W;  // 24-bit 전체 사용, 양수
```

- POWER는 24비트 전체가 의미 있음 (`bits 23-0`, RESERVED 없음)
- 양수 표현 (Table 7-13: "Unsigned representation. Positive value.")
- 부호확장 불필요, raw24 그대로 사용

### 11.5 `INA228_ReadDieTemp()`
**근거**: §7.6.1.7 Table 7-11 (p.25)

```c
int16_t signed_raw = (int16_t)raw;
*temp_C = signed_raw * 7.8125e-3f;
```

- 16비트 2의 보수 → `int16_t` 캐스팅으로 자동 부호확장
- 측정 범위: §6.3 *Recommended Operating Conditions* (p.4): –40 °C ~ +125 °C
- 정확도: §6.5 *Electrical Characteristics* (p.5): ±1°C @ 25°C

---

## 12. 누적값 읽기 함수 근거 (Energy/Charge)

### 12.1 `INA228_ReadEnergy()`
**근거**: §7.6.1.10 Table 7-14 (p.26) + **식 (6)** (p.31)

$$\text{Energy [J]} = 16 \times 3.2 \times \text{CURRENT\_LSB} \times \text{ENERGY register}$$

```c
*energy_J = (double)raw40 * INA228_ENERGY_LSB_J;
```

- 40비트 unsigned → uint64_t에 그대로 저장 (부호확장 불필요)
- double 사용 이유: 최대값 = 2⁴⁰ × ENERGY_LSB ≈ 1.69 × 10¹² × 16 × 3.2 × 30e-6 ≈ **1.69 × 10⁹ J** → float 정밀도 부족
- §7.3.2 (p.13): ENERGY는 변환 사이클마다 누적되므로 평균 적용 안 됨

### 12.2 `INA228_ReadCharge()`
**근거**: §7.6.1.11 Table 7-15 (p.26) + **식 (7)** (p.32)

$$\text{Charge [C]} = \text{CURRENT\_LSB} \times \text{CHARGE register}$$

```c
int64_t signed_raw = signed40_from_64(raw40);  // 40-bit 부호확장
*charge_C = (double)signed_raw * INA228_CHARGE_LSB_C;
```

- 40비트 **two's complement** → 음수(방전) 가능
- BMS SoC 추정의 핵심 입력: 쿨롱 카운팅을 INA228 하드웨어가 대신 수행해 줌
- §7.3.2: "The energy and charge values are accumulated for each conversion cycle."

### 12.3 `INA228_ResetAccumulators()`
**근거**: §7.6.1.1 Table 7-5 (p.22), `RSTACC` 비트 (bit 14)

```c
return ina228_write16(CONFIG, RSTACC | ADCRANGE_40MV);
```

- **중요**: CONFIG 레지스터에 단순히 RSTACC 비트만 쓰면 ADCRANGE가 0(±163.84mV)으로 reset됨
- 따라서 ADCRANGE_40MV도 함께 set해서 기존 설정 유지
- ENERGY/CHARGE만 초기화되고 measurement는 계속

### 12.4 Overflow 거동
- §8.1.2 (p.32):
  > *"Upon overflow, the ENERGY and CHARGE registers will roll over and start from zero."*
- 따라서 장시간 누적 시 주기적으로 읽어서 외부에서 누적 관리 필요
- 우리 시스템에서 ENERGY overflow까지 걸리는 시간 추정:
  - 최대 전력 ≈ 36V × 15A = 540 W
  - 2⁴⁰ × ENERGY_LSB = 1.69 × 10⁹ J / 540 W ≈ **3.13 × 10⁶ 초 ≈ 36일**
  - → 일반 운용에서 overflow 걱정 없음
- CHARGE overflow:
  - 2³⁹ × CURRENT_LSB = 5.5 × 10¹¹ × 30e-6 ≈ 1.65 × 10⁷ C
  - 15A 연속 방전 시: 1.65e7 / 15 ≈ **1.1 × 10⁶ 초 ≈ 12.7일**
  - → 마찬가지로 충분히 여유

---

## 13. INA228_Data 구조체와 INA228_ReadAll() 근거

### 13.1 설계 동기
데이터시트 §7.3.2 (p.13)의 내부 측정 엔진을 보면 한 conversion cycle에서 다음 값들이 **동시에** 갱신됨:

- VSHUNT, VBUS, DIETEMP (raw 측정)
- CURRENT (= VSHUNT × SHUNT_CAL 함수)
- POWER (= CURRENT × VBUS)
- ENERGY, CHARGE (이전 결과들의 누적)

즉, **이 값들은 서로 강하게 결합된 한 묶음의 스냅샷**이므로 한 번에 읽어 하나의 구조체에 담는 것이 데이터시트 모델과 일관됨. 사용자가 main에 개별 변수를 7개씩 선언하는 것보다 구조체 한 개로 다루는 것이 정합적.

### 13.2 `INA228_Data` 구조체 필드 매핑

```c
typedef struct {
    float  bus_voltage_V;     /* VBUS register   */
    float  shunt_voltage_mV;  /* VSHUNT register */
    float  current_A;         /* CURRENT register*/
    float  power_W;           /* POWER register  */
    float  die_temp_C;        /* DIETEMP register*/
    double charge_C;          /* CHARGE register */
    double energy_J;          /* ENERGY register */
} INA228_Data;
```

각 필드의 데이터시트 근거는 본 문서 §11, §12에서 이미 매핑된 것과 동일:

| 필드 | 타입 | 근거 | 비고 |
|---|---|---|---|
| `bus_voltage_V` | `float` | §7.6.1.6 Table 7-10, LSB=195.3125 µV | 항상 양수 |
| `shunt_voltage_mV` | `float` | §7.6.1.5 Table 7-9, LSB=78.125 nV (ADCRANGE=1) | 부호 있음 |
| `current_A` | `float` | §7.6.1.8 Table 7-12 + 식 (4) | 부호 있음 |
| `power_W` | `float` | §7.6.1.9 Table 7-13 + 식 (5) | 항상 양수 |
| `die_temp_C` | `float` | §7.6.1.7 Table 7-11, LSB=7.8125 m°C | 부호 있음 |
| `charge_C` | `double` | §7.6.1.11 Table 7-15 + 식 (7) | 40-bit 부호 있음, double 필요 |
| `energy_J` | `double` | §7.6.1.10 Table 7-14 + 식 (6) | 40-bit unsigned, double 필요 |

### 13.3 왜 charge/energy만 `double`인가
- ENERGY 최대값 = 2⁴⁰ × ENERGY_LSB ≈ 1.69 × 10⁹ J
- float (24-bit 가수)의 정밀도 한계: 약 ±1.6 × 10⁷까지만 정확
- 따라서 누적값이 큰 ENERGY와 CHARGE는 **double 필수** (이 결정도 §12.1, §12.2에서 이미 분석)

### 13.4 `INA228_ReadAll()` 구현 근거

이 함수는 7개의 개별 read 함수를 순차 호출하는 단순한 래퍼:

```c
INA228_Status INA228_ReadAll(INA228_Data *data)
{
    /* 순서:
     * 1. BusVoltage    →  VBUS  (0x05)
     * 2. ShuntVoltage  →  VSHUNT(0x04)
     * 3. Current       →  CURRENT(0x07)
     * 4. Power         →  POWER(0x08)
     * 5. DieTemp       →  DIETEMP(0x06)
     * 6. Charge        →  CHARGE(0x0A)
     * 7. Energy        →  ENERGY(0x09)
     */
}
```

**중요 사실**: 데이터시트 §7.5.1.1 *Writing to and Reading Through the I2C Serial Interface* (p.19)에 따르면 INA228은 **레지스터 단위 읽기**만 지원하며, **burst read(연속 다중 레지스터 자동 증가)는 지원하지 않음**. 따라서 7번의 I²C transaction이 필요한 것은 칩 자체의 제약 때문이지 라이브러리의 비효율이 아님.

### 13.5 읽기 순서 합리성
순서는 데이터시트 §7.3.2 Figure 7-2의 측정 흐름과 일관되게:

1. **즉시값(snapshot)** → `BusVoltage` → `ShuntVoltage` → `Current` → `Power` → `DieTemp`
2. **누적값(누적 카운터)** → `Charge` → `Energy`

이 순서로 읽으면 한 사이클 안에서 *최대한 시간적으로 가까운* 즉시값들이 먼저 수집됨. 누적값은 시간이 지나도 자체 누적이므로 마지막에 읽어도 무방.

### 13.6 에러 처리 정책
첫 번째 실패하는 read에서 즉시 return:

```c
st = INA228_ReadBusVoltage(&data->bus_voltage_V);
if (st != INA228_OK) return st;
/* ... 다음 read는 호출 안 됨 */
```

이유:
- I²C 통신이 한 번 실패하면 대부분 라인 문제(클럭 stretch 실패, 풀업 약함, EMI 등)로 후속 read도 실패할 확률 높음
- 부분적으로만 갱신된 구조체는 더 위험(예: 전압은 새 값, 전류는 옛 값으로 SoC 계산 시 오차 누적)
- 호출자가 `INA228_OK`인지만 검사하면 전체 스냅샷의 유효성 보장

### 13.7 전체 1회 호출 소요시간 추정
- I²C 100 kHz 기준 1바이트 ≈ 90 µs (start+9bit), Fast 400 kHz면 ≈ 22.5 µs
- 16-bit read: 주소(1) + 레지스터(1) + 데이터(2) ≈ 4바이트 transaction
- 24-bit read: 5바이트, 40-bit read: 7바이트
- 합계(400 kHz 기준): 약 (4×1 + 5×4 + 7×2) × 22.5 µs ≈ **850 µs**
- 10 ms CAN 주기 대비 충분히 짧음 → 폴링 부담 미미

---

## 14. 전체 신호 흐름과 평균/누적의 의미

### 14.1 INA228 내부 데이터 흐름 (데이터시트 §7.3.2, Figure 7-2, p.13)

```
ADC 측정 (T, i, v 순서 반복)
   │
   ├─→ VSHUNT, VBUS, DIETEMP 레지스터 (즉시 갱신 또는 평균 후 갱신)
   │
   ├─→ CURRENT 레지스터 (= VSHUNT × SHUNT_CAL의 함수)
   │
   ├─→ POWER 레지스터 (= CURRENT × VBUS)
   │
   ├─→ ENERGY 누적 (= POWER × T_cycle, 매 사이클 누적)
   │
   └─→ CHARGE 누적 (= CURRENT × T_cycle, 매 사이클 누적)
```

### 14.2 AVG가 적용되는 값 / 안 되는 값
- **AVG 적용**: VSHUNT, VBUS, DIETEMP, CURRENT, POWER (§7.3.2)
- **AVG 적용 안 됨**: ENERGY, CHARGE (§7.3.2)
  > *"The energy and charge values are accumulated for each conversion cycle. Therefore the INA228 averaging function is not applied to these."*

이는 매우 중요한 사실: **AVG를 크게 설정해도 ENERGY/CHARGE는 매 raw 샘플마다 누적되므로 SoC 계산에 영향 없음.**

### 14.3 연속 모드가 필수인 이유 재확인
- §7.3.3 (p.14):
  > *"In triggered mode, the accumulation registers (ENERGY and CHARGE) are invalid, as the device does not keep track of elapsed time."*
- 즉 SoC 계산을 위해서는 **반드시** MODE = 0xF (또는 9~E 중 연속) 사용
- 본 라이브러리는 `INA228_MODE_CONT_ALL`을 디폴트로 강제

---

## 부록 A. 데이터시트 인용 매트릭스

| 라이브러리 항목 | 데이터시트 위치 | 페이지 |
|---|---|---|
| I²C 7비트 주소 | §7.5, Table 7-2 | 19 |
| 레지스터 맵 | §7.6.1, Table 7-3 | 21 |
| Write/Read 프로토콜 | §7.5.1.1, Fig 7-7~9 | 19-20 |
| CONFIG 비트필드 | §7.6.1.1, Table 7-5 | 22 |
| ADC_CONFIG 비트필드 | §7.6.1.2, Table 7-6 | 22-23 |
| SHUNT_CAL 비트필드 | §7.6.1.3, Table 7-7 | 24 |
| VSHUNT 비트구조 | §7.6.1.5, Table 7-9 | 24-25 |
| VBUS 비트구조 | §7.6.1.6, Table 7-10 | 25 |
| DIETEMP 비트구조 | §7.6.1.7, Table 7-11 | 25 |
| CURRENT 비트구조 | §7.6.1.8, Table 7-12 | 25 |
| POWER 비트구조 | §7.6.1.9, Table 7-13 | 25-26 |
| ENERGY 비트구조 | §7.6.1.10, Table 7-14 | 26 |
| CHARGE 비트구조 | §7.6.1.11, Table 7-15 | 26 |
| MANUFACTURER_ID | §7.6.1.19, Table 7-23 | 29 |
| DEVICE_ID | §7.6.1.20, Table 7-24 | 29 |
| LSB 값 (Full Scale) | §8.1.1, Table 8-1 | 30 |
| **식 (2)** SHUNT_CAL | §8.1.2 | 31 |
| **식 (3)** CURRENT_LSB | §8.1.2 | 31 |
| **식 (4)** Current 계산 | §8.1.2 | 31 |
| **식 (5)** Power 계산 | §8.1.2 | 31 |
| **식 (6)** Energy 계산 | §8.1.2 | 31 |
| **식 (7)** Charge 계산 | §8.1.2 | 32 |
| Noise Performance | §8.1.3, Table 8-2 | 32 |
| Design Parameters | §8.2.1, Table 8-3 | 35 |
| Design Procedure | §8.2.2 | 35-36 |
| Internal Calc Engine | §7.3.2, Fig 7-2 | 13 |
| ADC 변환시간 설명 | §7.3.4 | 13-14 |
| 연속/Trigger 차이 | §7.3.3 | 14 |

---

## 부록 B. 본 라이브러리에서 **사용하지 않은** 데이터시트 기능 (확장 여지)

추후 한이음/공모전에서 기능 강조 필요 시 추가 가능한 기능들:

| 기능 | 데이터시트 | 활용처 |
|---|---|---|
| ALERT 인터럽트 | §7.4, §7.6.1.12 Table 7-16 | 과전류/과전압 즉시 차단 |
| 션트 온도보상 (TEMPCOMP/SHUNT_TEMPCO) | §7.3.5, §7.6.1.4 Table 7-8 | 정밀도 향상 (션트 온도 드리프트 보정) |
| SOVL/SUVL/BOVL/BUVL/TOL/POL | §7.6.1.13~18 | 하드웨어 보호 임계값 |
| 변환 지연 (CONVDLY) | §7.6.1.1 (CONFIG bits 13-6) | 다중 INA228 동기화 |
| SMBus Alert Response | §7.5.1.3 | 빠른 fault 식별 |
| High-Speed I²C (2.94 MHz) | §7.5.1.2 | 고속 데이터 수집 |

---

## 변경 이력

| 버전 | 날짜 | 변경 사항 |
|---|---|---|
| 1.0 | 2026-05-20 | 초기 작성. SLYS021A (Rev. May 2022) 기준 전체 매핑 완료 |
| 1.1 | 2026-05-20 | `INA228_Data` 구조체 + `INA228_ReadAll()` API 추가. §13 신설, 기존 §13 → §14로 번호 변경 |
