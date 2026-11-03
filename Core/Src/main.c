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


