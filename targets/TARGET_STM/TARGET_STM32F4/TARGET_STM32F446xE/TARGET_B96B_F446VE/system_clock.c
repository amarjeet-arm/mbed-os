/* mbed Microcontroller Library
* Copyright (c) 2006-2017 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
  * This file configures the system clock as follows:
  *-----------------------------------------------------------------------------
  * System clock source                | 1- PLL_HSE_XTAL        | 3- PLL_HSI
  *                                    | (external 16 MHz xtal) | (internal 16 MHz)
  *-----------------------------------------------------------------------------
  * SYSCLK(MHz)                        | 180                    | 180
  *-----------------------------------------------------------------------------
  * AHBCLK (MHz)                       | 180                    | 180
  *-----------------------------------------------------------------------------
  * APB1CLK (MHz)                      |  45                    |  45
  *-----------------------------------------------------------------------------
  * APB2CLK (MHz)                      |  90                    |  90
  *-----------------------------------------------------------------------------
  * USB capable (48 MHz precise clock) | YES                    |  NO
  *-----------------------------------------------------------------------------
**/

#include "stm32f4xx.h"

/* Select the clock sources (other than HSI) to start with (0=OFF, 1=ON) */
#define USE_PLL_HSE_EXTC (0) /* Use external clock */
#define USE_PLL_HSE_XTAL (1) /* Use external xtal */

#define DEBUG_MCO        (0) // 0=OFF
// 1=Output the MCO1 on PA8
// 2=Output the MCO2 on PC9


#if (USE_PLL_HSE_XTAL != 0) || (USE_PLL_HSE_EXTC != 0)
uint8_t SetSysClock_PLL_HSE(uint8_t bypass);
#endif

uint8_t SetSysClock_PLL_HSI(void);


/**
  * @brief  Configures the System clock source, PLL Multiplier and Divider factors,
  *               AHB/APBx prescalers and Flash settings
  * @note   This function should be called only once the RCC clock configuration
  *         is reset to the default reset state (done in SystemInit() function).
  * @param  None
  * @retval None
  */
void SetSysClock(void)
{
    /* 1- Try to start with HSE and external clock */
#if USE_PLL_HSE_EXTC != 0
    if (SetSysClock_PLL_HSE(1) == 0)
#endif
    {
        /* 2- If fail try to start with HSE and external xtal */
#if USE_PLL_HSE_XTAL != 0
        if (SetSysClock_PLL_HSE(0) == 0)
#endif
        {
            /* 3- If fail start with HSI clock */
            if (SetSysClock_PLL_HSI() == 0) {
                while (1) {
                    // [TODO] Put something here to tell the user that a problem occured...
                }
            }
        }
    }

    // Output clock on MCO2 pin(PC9) for debugging purpose
#if DEBUG_MCO == 2
    HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_SYSCLK, RCC_MCODIV_4); // 180/4 = 45 MHz
#endif
}

#if (USE_PLL_HSE_XTAL != 0) || (USE_PLL_HSE_EXTC != 0)
/******************************************************************************/
/*            PLL (clocked by HSE) used as System clock source                */
/******************************************************************************/
uint8_t SetSysClock_PLL_HSE(uint8_t bypass)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    /* The voltage scaling allows optimizing the power consumption when the device is
       clocked below the maximum system frequency, to update the voltage scaling value
       regarding system frequency refer to product datasheet. */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Enable HSE oscillator and activate PLL with HSE as source
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    if (bypass == 0) {
        RCC_OscInitStruct.HSEState          = RCC_HSE_ON; // External 8 MHz xtal on OSC_IN/OSC_OUT
    } else {
        RCC_OscInitStruct.HSEState          = RCC_HSE_BYPASS; // External 8 MHz clock on OSC_IN
    }

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 16;            // VCO input clock = 1 MHz (16 MHz / 16)
    RCC_OscInitStruct.PLL.PLLN = 360;           // VCO output clock = 360 MHz (1 MHz * 360)
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; // PLLCLK = 180 MHz (360 MHz / 2)
    RCC_OscInitStruct.PLL.PLLQ = 7;             //
    RCC_OscInitStruct.PLL.PLLR = 6;             //
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        return 0; // FAIL
    }

    // Activate the OverDrive to reach the 180 MHz Frequency
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        return 0; // FAIL
    }

    // Select PLLSAI output as USB clock source
    PeriphClkInitStruct.PLLSAI.PLLSAIM = 8;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
    PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLSAIP;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    // Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 180 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  //  45 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  //  90 MHz
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        return 0; // FAIL
    }

    // Output clock on MCO1 pin(PA8) for debugging purpose
#if DEBUG_MCO == 1
    if (bypass == 0) {
        HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);    // 16 MHz with xtal
    } else {
        HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);    // xx MHz with external clock (MCO)
    }
#endif

    return 1; // OK
}
#endif

/******************************************************************************/
/*            PLL (clocked by HSI) used as System clock source                */
/******************************************************************************/
uint8_t SetSysClock_PLL_HSI(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* The voltage scaling allows optimizing the power consumption when the device is
       clocked below the maximum system frequency, to update the voltage scaling value
       regarding system frequency refer to product datasheet. */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Enable HSI oscillator and activate PLL with HSI as source
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSEState            = RCC_HSE_OFF;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 16;            // VCO input clock = 1 MHz (16 MHz / 16)
    RCC_OscInitStruct.PLL.PLLN            = 360;           // VCO output clock = 360 MHz (1 MHz * 360)
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2; // PLLCLK = 180 MHz (360 MHz / 2)
    RCC_OscInitStruct.PLL.PLLQ            = 7;             //
    RCC_OscInitStruct.PLL.PLLQ            = 6;             //
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        return 0; // FAIL
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType      = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK; // 180 MHz
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;         // 180 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;           //  45 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;           //  90 MHz
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        return 0; // FAIL
    }

    // Output clock on MCO1 pin(PA8) for debugging purpose
#if DEBUG_MCO == 1
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1); // 16 MHz
#endif

    return 1; // OK
}
