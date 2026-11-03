#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ===== 周邊變數 ===== */
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* ===== LED 狀態 ===== */
#define STATE_GREEN   0
#define STATE_YELLOW  1
#define STATE_RED     2
#define STATE_FAILED  3

uint8_t led_state = STATE_YELLOW;

/* ===== 遊戲變數 ===== */
uint8_t correct_answer = 0;
uint8_t game_started = 0;
uint8_t game_finished = 0;
uint8_t remaining_time = 30;
uint8_t question_count = 0;
uint8_t answer_locked = 0;

uint32_t last_second_tick = 0;
uint32_t last_led_tick = 0;
uint32_t last_button_tick = 0;

/* ===== UART 接收 ===== */
uint8_t rx_data;
char rx_buffer[32];
uint8_t rx_index = 0;

/* ===== 按鈕 ===== */
GPIO_PinState last_pa8 = GPIO_PIN_SET;
GPIO_PinState last_pa9 = GPIO_PIN_SET;
GPIO_PinState last_pa10 = GPIO_PIN_SET;

/* ===== LED 閃爍 ===== */
uint8_t red_led_on = 0;
uint32_t breath_tick = 0;
uint16_t breath_value = 0;
int16_t breath_direction = 1;
/* ===== 函式宣告 ===== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);

void Error_Handler(void);

void LED_Update(void);
void Breath_LED_Update(void);
void Game_Reset(void);
void Game_Check_Answer(uint8_t answer);
void Game_Timer_Update(void);
void Button_Check(void);
void UART_Receive_Update(void);
void UART_Process_Command(char *command);
void UART_Send_Text(char *text);
void UART_Send_Time(void);

/* ===== UART 傳送文字 ===== */
void UART_Send_Text(char *text)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)text,
                      strlen(text),
                      100);
}

/* ===== 傳送剩餘時間 ===== */
void UART_Send_Time(void)
{
    char message[20];

    sprintf(message, "T,%d\n", remaining_time);

    UART_Send_Text(message);
}

/* ===== LED 更新 ===== */
void LED_Update(void)
{
    if (led_state == STATE_GREEN)
    {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 999);
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);

        if (led_state == STATE_YELLOW)
        {
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, breath_value);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        }
        else if (led_state == STATE_RED || led_state == STATE_FAILED)
        {
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, breath_value);
        }
        else
        {
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        }
    }
}
/* ===== 遊戲重置 ===== */
void Game_Reset(void)
{
	game_started = 0;
	    game_finished = 0;

	    remaining_time = 30;
	    question_count = 0;
	    answer_locked = 0;

	    led_state = STATE_YELLOW;
	    red_led_on = 0;

	    LED_Update();
}

/* ===== 判斷答案 ===== */
void Game_Check_Answer(uint8_t answer)
{
	if (game_started == 0 ||
	        game_finished == 1 ||
	        answer_locked == 1)
	    {
	        return;
	    }

	    answer_locked = 1;

	    if (answer == correct_answer)
	    {
	        question_count++;

	        if (question_count >= 3)
	        {
	            game_started = 0;
	            game_finished = 1;

	            led_state = STATE_GREEN;
	            red_led_on = 0;

	            UART_Send_Text("R,CORRECT\n");
	            UART_Send_Text("S,WIN\n");
	        }
	        else
	        {
	            led_state = STATE_GREEN;
	            UART_Send_Text("R,CORRECT\n");
	        }
	    }
	    else
	    {
	        game_started = 0;
	        game_finished = 1;

	        led_state = STATE_FAILED;
	        red_led_on = 1;

	        UART_Send_Text("R,WRONG\n");
	    }

	    LED_Update();
}

/* ===== 倒數計時 ===== */
void Game_Timer_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (game_started == 0)
    {
        return;
    }

    if (now - last_second_tick >= 1000)
    {
        last_second_tick += 1000;

        if (remaining_time > 0)
        {
            remaining_time--;

            UART_Send_Time();
        }

        if (remaining_time == 0)
        {
            game_started = 0;
            game_finished = 1;

            led_state = STATE_FAILED;
            red_led_on = 1;

            UART_Send_Text("R,TIMEOUT\n");

            LED_Update();
        }
    }
}
/* ===== 紅燈加速閃爍 ===== */
void Red_LED_Update(void)
{
    if (led_state != STATE_RED &&
        led_state != STATE_FAILED)
    {
        return;
    }

    uint32_t now = HAL_GetTick();

    uint32_t interval = 500;

    if (remaining_time <= 20)
        interval = 350;

    if (remaining_time <= 10)
        interval = 180;

    if (remaining_time <= 5)
        interval = 80;

    if (now - last_led_tick >= interval)
    {
        last_led_tick = now;

        red_led_on = !red_led_on;

        LED_Update();
    }
}
/* ===== 黃燈、紅燈呼吸效果 ===== */
void Breath_LED_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (now - breath_tick >= 5)
    {
        breath_tick = now;

        breath_value += breath_direction * 5;

        if (breath_value >= 999)
        {
            breath_value = 999;
            breath_direction = -1;
        }
        else if (breath_value <= 0)
        {
            breath_value = 0;
            breath_direction = 1;
        }

        LED_Update();
    }
}
/* ===== 按鈕偵測 ===== */
void Button_Check(void)
{
    uint32_t now = HAL_GetTick();

    GPIO_PinState pa8 =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);

    GPIO_PinState pa9 =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9);

    GPIO_PinState pa10 =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);

    if (now - last_button_tick >= 250)
    {
        if (last_pa8 == GPIO_PIN_SET &&
            pa8 == GPIO_PIN_RESET)
        {
            Game_Check_Answer(1);
            last_button_tick = now;
        }
        else if (last_pa9 == GPIO_PIN_SET &&
                 pa9 == GPIO_PIN_RESET)
        {
            Game_Check_Answer(2);
            last_button_tick = now;
        }
        else if (last_pa10 == GPIO_PIN_SET &&
                 pa10 == GPIO_PIN_RESET)
        {
            Game_Check_Answer(3);
            last_button_tick = now;
        }
    }

    last_pa8 = pa8;
    last_pa9 = pa9;
    last_pa10 = pa10;
}

/* ===== 處理 MSYS2 指令 ===== */
void UART_Process_Command(char *command)
{
    if (command[0] == 'Q' && command[1] == ',')
    {
        uint8_t answer = atoi(&command[2]);

        if (answer >= 1 && answer <= 3)
        {
            // 新的一場遊戲
            if (game_finished == 1 || question_count == 0 && game_started == 0)
            {
                if (game_finished == 1)
                {
                    Game_Reset();
                }

                if (game_started == 0 && question_count == 0)
                {
                    remaining_time = 30;
                    last_second_tick = HAL_GetTick();
                    game_started = 1;
                }
            }

            // 更新本題答案，不重設倒數
            correct_answer = answer;
            answer_locked = 0;

            UART_Send_Time();
        }
    }

    else if (command[0] == 'A' && command[1] == ',')
    {
        uint8_t answer = atoi(&command[2]);

        if (answer >= 1 && answer <= 3)
        {
            Game_Check_Answer(answer);
        }
    }
}
/* ===== UART 非阻塞接收 ===== */
void UART_Receive_Update(void)
{
    if (HAL_UART_Receive(&huart2, &rx_data, 1, 1) == HAL_OK)
    {
        if (rx_data == '\n' || rx_data == '\r')
        {
            if (rx_index > 0)
            {
                rx_buffer[rx_index] = '\0';

                UART_Process_Command(rx_buffer);

                rx_index = 0;
            }
        }
        else
        {
            if (rx_index < sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_index++] = rx_data;
            }
            else
            {
                rx_index = 0;
            }
        }
    }
}

/* ===== 主程式 ===== */
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();

    MX_USART2_UART_Init();

    MX_TIM2_Init();

    MX_TIM3_Init();

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK)
        Error_Handler();

    Game_Reset();

    while (1)
    {
    	 UART_Receive_Update();
    	    Button_Check();
    	    Game_Timer_Update();
    	    Breath_LED_Update();
}
}
// UART_Receive_Update();
//Button_Check();
//Game_Timer_Update();
//Red_LED_Update();

/* ===== 系統時鐘 ===== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(
        PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
        &RCC_ClkInitStruct,
        FLASH_LATENCY_0) != HAL_OK)
        Error_Handler();
}
/* ===== TIM2 黃燈 PA5 ===== */
static void MX_TIM2_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    TIM_OC_InitTypeDef sConfigOC = {0};

    htim2.Instance = TIM2;

    htim2.Init.Prescaler = 169;

    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;

    htim2.Init.Period = 999;

    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
        Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;

    sMasterConfig.MasterSlaveMode =
        TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(
        &htim2, &sMasterConfig) != HAL_OK)
        Error_Handler();

    sConfigOC.OCMode = TIM_OCMODE_PWM1;

    sConfigOC.Pulse = 0;

    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;

    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(
        &htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();

    HAL_TIM_MspPostInit(&htim2);
}

/* ===== TIM3 紅燈 PA6、綠燈 PA7 ===== */
static void MX_TIM3_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    TIM_OC_InitTypeDef sConfigOC = {0};

    htim3.Instance = TIM3;

    htim3.Init.Prescaler = 169;

    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;

    htim3.Init.Period = 999;

    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    htim3.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
        Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;

    sMasterConfig.MasterSlaveMode =
        TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(
        &htim3, &sMasterConfig) != HAL_OK)
        Error_Handler();

    sConfigOC.OCMode = TIM_OCMODE_PWM1;

    sConfigOC.Pulse = 0;

    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;

    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(
        &htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();

    if (HAL_TIM_PWM_ConfigChannel(
        &htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
        Error_Handler();

    HAL_TIM_MspPostInit(&htim3);
}

/* ===== USART2 UART ===== */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;

    huart2.Init.BaudRate = 115200;

    huart2.Init.WordLength = UART_WORDLENGTH_8B;

    huart2.Init.StopBits = UART_STOPBITS_1;

    huart2.Init.Parity = UART_PARITY_NONE;

    huart2.Init.Mode = UART_MODE_TX_RX;

    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;

    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    huart2.Init.OneBitSampling =
        UART_ONE_BIT_SAMPLE_DISABLE;

    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;

    huart2.AdvancedInit.AdvFeatureInit =
        UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();

    if (HAL_UARTEx_SetTxFifoThreshold(
        &huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
        Error_Handler();

    if (HAL_UARTEx_SetRxFifoThreshold(
        &huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
        Error_Handler();

    if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
        Error_Handler();
}

/* ===== GPIO 按鈕 ===== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin =
        GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ===== 錯誤處理 ===== */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
}

#endif




