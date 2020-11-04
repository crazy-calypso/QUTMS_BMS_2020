/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "bq769x0.h"
#include "BMS_CAN_Messages.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define N_CELLS	10

// OVRD_ALERT, SCD, OCD
#define SYS_STAT_FLAG_BITS 0b00010011
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void SystemClock_8MHz_Config(void);

uint8_t GetHardwareID();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	//MX_I2C1_Init();
	//MX_USART1_UART_Init();
	//MX_CAN_Init();
	/* USER CODE BEGIN 2 */

	// bring out of shipping mode
	// full clock speed
	SystemClock_8MHz_Config();

	// initialize peripherals
	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_USART1_UART_Init();
	MX_CAN_Init();

	// Initialize CAN
	Configure_CAN(&hcan);

	// both on to signify we vibing
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

	// check BQ for faults
	uint8_t sys_stat = 0;
	HAL_StatusTypeDef BQ_result = HAL_BUSY;

	BQ_result = bq769x0_reg_read_byte(&hi2c1, BQ_SYS_STAT, &sys_stat);

	if (BQ_result != HAL_OK) {
		// error, couldn't talk to BQ chip??
		Error_Handler();
	}

	if (sys_stat & SYS_STAT_FLAG_BITS > 0) {
		// fault pins are set whats the deal

		// only clear pins we care about
		sys_stat = sys_stat & SYS_STAT_FLAG_BITS;
		BQ_result = bq769x0_reg_write_byte(&hi2c1, BQ_SYS_STAT, sys_stat);

		if (BQ_result != HAL_OK) {
			// error, couldn't talk to BQ chip??
			Error_Handler();
		}

		// confirm bits are cleared
		BQ_result = bq769x0_reg_read_byte(&hi2c1, BQ_SYS_STAT, &sys_stat);

		if ((BQ_result != HAL_OK) || (sys_stat & SYS_STAT_FLAG_BITS) != 0) {
			// couldn't talk to BQ or bits are still set
			Error_Handler();
		}
	}

	// both off to signify we vibing
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

	// enable DSG
	BQ_result = bq769x0_set_DSG(&hi2c1, 1);
	if (BQ_result != HAL_OK) {
		// error, couldn't talk to BQ chip??
		Error_Handler();
	}

	// check fault pin to make sure we're all good before starting main procedure
	if(HAL_GPIO_ReadPin(CELL_ALERT_GPIO_Port, CELL_ALERT_Pin) != GPIO_PIN_RESET) {
		// pin is high whats going on
		Error_Handler();
	}

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */

	uint16_t voltages[4];
	voltages[0] = 0;
	voltages[1] = 1;
	voltages[2] = 2;
	voltages[3] = 3;

	BMS_TransmitVoltage_t voltage_msg = Compose_BMS_TransmitVoltage(2, 3,
			voltages);
	CAN_TxHeaderTypeDef header = { .ExtId = voltage_msg.id, .IDE = CAN_ID_EXT,
			.RTR = CAN_RTR_DATA, .DLC = sizeof(voltage_msg.data),
			.TransmitGlobalTime = DISABLE, };

	uint32_t txMailbox = 0;

	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
		HAL_CAN_AddTxMessage(&hcan, &header, voltage_msg.data, &txMailbox);
		HAL_Delay(1000);
		HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
		HAL_Delay(1000);
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV8;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1
			| RCC_PERIPHCLK_I2C1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */
/**
 * @brief System Clock Configuration for 8MHz based on generated config.
 * @retval None
 */
void SystemClock_8MHz_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI; // (0x00000002U)
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;				  // (0x1UL << (0U))
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;	// (0x10U)
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;			   // (0x00000000U)
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;	// (0x00000002U)|(0x00000001U)|(0x00000004U)
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;	// (0x00000000U)
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;// (0x00000000U) //DIVIDER is primary diff
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;		// (0x00000000U)

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {// {struct}, (0x00000000U)
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1
			| RCC_PERIPHCLK_I2C1;	// (0x00000001U)| (0x00000020U)
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;	// (0x00000000U)
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;	// (0x00000000U)
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
}

/*
 * Return the ID set but Hardware.
 * TODO: Requires usage of all pins.
 */
uint8_t GetHardwareID() {
	return 0;						//(HAL_GPIO_ReadPin(GPIOB, ID1_Pin));
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
	/* User can add his own implementation to report the HAL error return state */
	while (1) {
		HAL_Delay(100);
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
