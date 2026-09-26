/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
uint32_t defaultTaskBuffer[128];
osStaticThreadDef_t defaultTaskControlBlock;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .cb_mem = &defaultTaskControlBlock,
    .cb_size = sizeof(defaultTaskControlBlock),
    .stack_mem = &defaultTaskBuffer[0],
    .stack_size = sizeof(defaultTaskBuffer),
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for KeyTask */
osThreadId_t KeyTaskHandle;
uint32_t KeyTaskBuffer[128];
osStaticThreadDef_t KeyTaskControlBlock;
const osThreadAttr_t KeyTask_attributes = {
    .name = "KeyTask",
    .cb_mem = &KeyTaskControlBlock,
    .cb_size = sizeof(KeyTaskControlBlock),
    .stack_mem = &KeyTaskBuffer[0],
    .stack_size = sizeof(KeyTaskBuffer),
    .priority = (osPriority_t)osPriorityHigh,
};
/* Definitions for lcdTask */
osThreadId_t lcdTaskHandle;
uint32_t lcdTaskBuffer[192];
osStaticThreadDef_t lcdTaskControlBlock;
const osThreadAttr_t lcdTask_attributes = {
    .name = "lcdTask",
    .cb_mem = &lcdTaskControlBlock,
    .cb_size = sizeof(lcdTaskControlBlock),
    .stack_mem = &lcdTaskBuffer[0],
    .stack_size = sizeof(lcdTaskBuffer),
    .priority = (osPriority_t)osPriorityBelowNormal,
};
/* Definitions for calculatorTask */
osThreadId_t calculatorTaskHandle;
uint32_t claculatorTaskBuffer[512];
osStaticThreadDef_t claculatorTaskControlBlock;
const osThreadAttr_t calculatorTask_attributes = {
    .name = "calculatorTask",
    .cb_mem = &claculatorTaskControlBlock,
    .cb_size = sizeof(claculatorTaskControlBlock),
    .stack_mem = &claculatorTaskBuffer[0],
    .stack_size = sizeof(claculatorTaskBuffer),
    .priority = (osPriority_t)osPriorityLow,
};
/* Definitions for controllerTask */
osThreadId_t controllerTaskHandle;
uint32_t controllerTaskBuffer[256];
osStaticThreadDef_t controllerTaskControlBlock;
const osThreadAttr_t controllerTask_attributes = {
    .name = "controllerTask",
    .cb_mem = &controllerTaskControlBlock,
    .cb_size = sizeof(controllerTaskControlBlock),
    .stack_mem = &controllerTaskBuffer[0],
    .stack_size = sizeof(controllerTaskBuffer),
    .priority = (osPriority_t)osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void KeyTaskFunc(void *argument);
void lcdTaskFunc(void *argument);
void calculatorTaskFunc(void *argument);
void controllerTaskFunc(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created. It is also called by various parts of the
    demo application. If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* creation of KeyTask */
    KeyTaskHandle = osThreadNew(KeyTaskFunc, NULL, &KeyTask_attributes);

    /* creation of lcdTask */
    lcdTaskHandle = osThreadNew(lcdTaskFunc, NULL, &lcdTask_attributes);

    /* creation of calculatorTask */
    calculatorTaskHandle = osThreadNew(calculatorTaskFunc, NULL, &calculatorTask_attributes);

    /* creation of controllerTask */
    controllerTaskHandle = osThreadNew(controllerTaskFunc, NULL, &controllerTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
    /* init code for USB_DEVICE */
    MX_USB_DEVICE_Init();
    /* USER CODE BEGIN StartDefaultTask */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_KeyTaskFunc */
/**
 * @brief Function implementing the KeyTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_KeyTaskFunc */
void KeyTaskFunc(void *argument)
{
    /* USER CODE BEGIN KeyTaskFunc */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END KeyTaskFunc */
}

/* USER CODE BEGIN Header_lcdTaskFunc */
/**
 * @brief Function implementing the lcdTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_lcdTaskFunc */
void lcdTaskFunc(void *argument)
{
    /* USER CODE BEGIN lcdTaskFunc */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END lcdTaskFunc */
}

/* USER CODE BEGIN Header_calculatorTaskFunc */
/**
 * @brief Function implementing the calculatorTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_calculatorTaskFunc */
void calculatorTaskFunc(void *argument)
{
    /* USER CODE BEGIN calculatorTaskFunc */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END calculatorTaskFunc */
}

/* USER CODE BEGIN Header_controllerTaskFunc */
/**
 * @brief Function implementing the controllerTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_controllerTaskFunc */
void controllerTaskFunc(void *argument)
{
    /* USER CODE BEGIN controllerTaskFunc */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END controllerTaskFunc */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
