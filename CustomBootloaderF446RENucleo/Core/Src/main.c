/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include<stdio.h>
#include<string.h>
//#define IAP_DEMO 1
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

//#if IAP_DEMO
typedef void (*pFunction)(void);
//#endif
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//#if IAP_DEMO
#define FLASH_APP_ADDR 0x8008000
//#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
typedef void(*ptrF) (uint32_t dlyticks);
typedef void(*pFunction) (void);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
unsigned char __attribute__((section(".myBufSectionRAM"))) buf_ram[128];
__attribute__((section(".myBufSectionFLASH")))
volatile uint8_t buf_flash[10] = { 0,1,2,3,4,5,6,7,8,9 };

uint8_t __attribute__((section(".myBufSectionFLASH"))) myFlashBuffer[256];

#define LED_GREEN_Pin GPIO_PIN_5
#define LED_GREEN_GPIO_Port GPIOA

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
void go2APP(void);
/* USER CODE BEGIN PFP */
void blink(uint32_t dlyticks){
	HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port,LED_GREEN_Pin);
	HAL_Delay(dlyticks);
}

static ptrF functions[]={
		blink
};
//#if IAP_DEMO
void go2APP(void);
//#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void __attribute__ ((__section__(".RamFunc"))) TurnOnLED(GPIO_PinState PinState){
	if(PinState != GPIO_PIN_RESET){
		LED_GREEN_GPIO_Port->BSRR= (uint32_t) LED_GREEN_Pin;
	}
	else{
		LED_GREEN_GPIO_Port->BSRR= (uint32_t) LED_GREEN_Pin << 16;
	}
}

typedef void (*pFunction)(void);

void go2APP(void) {
    // Define the start address of your application in Flash
    // (Make sure this matches where your user application is linked, e.g., 0x08008000 or 0x08010000)
    uint32_t app_address = 0x08008000;

    // 1. Check if a valid stack pointer exists at the application address (must be in RAM 0x20000000)
    uint32_t msp = *(__IO uint32_t*)app_address;
    if ((msp >= 0x20000000) && (msp <= 0x20020000)) {

        // 2. Get the Reset Handler entry address from the vector table (+4 offset)
        uint32_t jump_address = *(__IO uint32_t*)(app_address + 4);
        pFunction jump_to_application = (pFunction)jump_address;

        // 3. Disable all active interrupts and de-initialize HAL/Peripherals
        HAL_RCC_DeInit();
        HAL_DeInit();

        // Disable global interrupts
        __disable_irq();

        // 4. Set the Main Stack Pointer (MSP) to the application's stack value
        __set_MSP(*(__IO uint32_t*)app_address);

        // 5. Jump to the application's Reset Handler
        jump_to_application();
    }
}

int __write(int file, char *ptr, int len){
	int DataIdx;

	for(DataIdx=0;DataIdx<len;DataIdx++){
		HAL_UART_Transmit(&huart2, (uint8_t *) ptr++, 1, 100);
	}
	return len;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
//	#if IAP_DEMO
  	  printf("IAP DEMO BOOT\r\n");
//	#endif

    	char *msg = "Bootloader Started!\r\n";
    	HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);

    	char *prompt_msg = "Press and hold Blue Button (B1) to enter Bootloader mode. Booting app in 3s...\r\n";
    	HAL_UART_Transmit(&huart2, (uint8_t*)prompt_msg, strlen(prompt_msg), HAL_MAX_DELAY);

    	uint32_t  start_tick=HAL_GetTick();
    	uint8_t stay_in_bootloader=0;

    	while(HAL_GetTick()-start_tick<3000){
    		// Check if B1 button is pressed (Nucleo button pulls pin to GND / GPIO_PIN_RESET when pressed)
    		      if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
    		      {
    		          stay_in_bootloader = 1;
    		          break;
    		      }
    	}
    	if (stay_in_bootloader)
    	  {
    	      char *stay_msg = "-> Bootloader Mode Active (Type 'help' for commands).\r\n > ";
    	      HAL_UART_Transmit(&huart2, (uint8_t*)stay_msg, strlen(stay_msg), HAL_MAX_DELAY);

    	      char cmd_buffer[32];
    	      memset(cmd_buffer, 0, sizeof(cmd_buffer));
    	      uint8_t idx = 0;

    	      while (1)
    	      {
    	          uint8_t rx_byte;
    	          // Wait for a character from UART (with a short timeout so we can still do background tasks if needed)
    	          if (HAL_UART_Receive(&huart2, &rx_byte, 1, 100) == HAL_OK)
    	          {
    	              // Echo the character back so you can see what you're typing
    	              HAL_UART_Transmit(&huart2, &rx_byte, 1, HAL_MAX_DELAY);

    	              // Handle Backspace / Delete
    	              if (rx_byte == '\b' || rx_byte == 127)
    	              {
    	                  if (idx > 0)
    	                  {
    	                      idx--;
    	                      cmd_buffer[idx] = '\0';
    	                  }
    	              }
    	              // Handle Enter (Carriage Return or Line Feed)
    	              else if (rx_byte == '\r' || rx_byte == '\n')
    	              {
    	                  char *newline = "\r\n";
    	                  HAL_UART_Transmit(&huart2, (uint8_t*)newline, strlen(newline), HAL_MAX_DELAY);

    	                  // Null-terminate the string
    	                  cmd_buffer[idx] = '\0';

    	                  // --- PARSE COMMANDS ---
    	                  if (strcmp(cmd_buffer, "help") == 0 || strcmp(cmd_buffer, "?") == 0)
    	                  {
    	                      char *help_text = "Available commands:\r\n"
    	                                        "  help     - Show this help menu\r\n"
    	                                        "  version  - Display bootloader version\r\n"
    	                                        "  jump     - Jump to user application\r\n"
    	                                        "  erase    - Erase application sector (stub)\r\n";
    	                      HAL_UART_Transmit(&huart2, (uint8_t*)help_text, strlen(help_text), HAL_MAX_DELAY);
    	                  }
    	                  else if (strcmp(cmd_buffer, "version") == 0)
    	                  {
    	                      char *ver_text = "STM32F446RE Custom Bootloader v1.0\r\n";
    	                      HAL_UART_Transmit(&huart2, (uint8_t*)ver_text, strlen(ver_text), HAL_MAX_DELAY);
    	                  }
    	                  else if (strcmp(cmd_buffer, "jump") == 0)
    	                  {
    	                      char *jump_text = "Attempting manual jump to application...\r\n";
    	                      HAL_UART_Transmit(&huart2, (uint8_t*)jump_text, strlen(jump_text), HAL_MAX_DELAY);
    	                      go2APP();
    	                      char *fail_text = "Jump failed: No valid application found at 0x08008000.\r\n";
    	                      HAL_UART_Transmit(&huart2, (uint8_t*)fail_text, strlen(fail_text), HAL_MAX_DELAY);
    	                  }
    	                  else if (strcmp(cmd_buffer, "erase") == 0)
    	                  {
    	                      char *erase_text = "Flash erase routine not yet implemented.\r\n";
    	                      HAL_UART_Transmit(&huart2, (uint8_t*)erase_text, strlen(erase_text), HAL_MAX_DELAY);
    	                  }
    	                  else if (strlen(cmd_buffer) > 0)
    	                  {
    	                      char *err_text = "Unknown command. Type 'help' for options.\r\n";
    	                      HAL_UART_Transmit(&huart2, (uint8_t*)err_text, strlen(err_text), HAL_MAX_DELAY);
    	                  }

    	                  // Reset buffer for the next command
    	                  memset(cmd_buffer, 0, sizeof(cmd_buffer));
    	                  idx = 0;

    	                  char *prompt = " > ";
    	                  HAL_UART_Transmit(&huart2, (uint8_t*)prompt, strlen(prompt), HAL_MAX_DELAY);
    	              }
    	              else
    	              {
    	                  // Append character to buffer if there's space
    	                  if (idx < sizeof(cmd_buffer) - 1)
    	                  {
    	                      cmd_buffer[idx++] = rx_byte;
    	                  }
    	              }
    	          }
    	      }
    	  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  (*functions[0]) (1000);
	  HAL_Delay(500);
	  (*functions[0]) (1000);
    /* USER CODE BEGIN 3 */
//	#if IAP_DEMO
  	  printf("IAP DEMO BOOT\r\n");
//	#endif
//  	go2APP();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
