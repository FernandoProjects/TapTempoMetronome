/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ssd1306.h"
#include "ssd1306_tests.h"
#include <stdbool.h>
#include "ssd1306_fonts.h"

/* Constant macros -----------------------------------------------------------*/
#define ENCODER_DEBOUNCE_MS 30
#define TAP_MAX_COUNT 6
#define TAP_TIMEOUT_MS 2500 
#define DAC_RESOLUTION 4095
#define MAX_SAFE_VOLUME_SCALE 2048
#define DEBOUNCE_MS 200
#define START_TICKING_TIMEOUT 1500
#define UI_TIMEOUT_MS 3000

/* Global constants -----------------------------------------------------------*/
const uint16_t downbeat_pulse[24] = {
  0, 600, 2000, 3500, 4800, 5800, 6500, 7000, 7300, 7500,
  7600, 7700, 7700, 7500, 7000, 6500, 5800, 5000, 4000, 2500,
  1200, 500, 100, 0
};
const uint16_t regular_pulse[16] = {
  0, 400, 1200, 2500, 4000, 5200, 6000, 6500,
  6500, 6000, 5200, 4000, 2500, 1200, 400, 0
};
const uint16_t subdivision_pulse[10] = {
  0, 200, 800, 1600, 2400, 2000, 1200, 600, 200, 0
};
const uint16_t volume_lookup[] = {64, 128, 256, 512, 768, 1024, 1536, 2048, 2048};
const unsigned char quarter[] = {
	0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 
	0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xdf, 
	0xc0, 0xff, 0xdf, 0xc0, 0xfe, 0x9f, 0xc0, 0xf0, 0x1f, 0xc0, 0xf0, 0x1f, 0xc0, 0xf0, 0x1f, 0xc0, 
	0xf0, 0x1f, 0xc0, 0xfc, 0x7f, 0xc0
};
const unsigned char eighth[] = {
	0xff, 0x7f, 0xc0, 0xff, 0x3f, 0xc0, 0xff, 0x3f, 0xc0, 0xff, 0x0f, 0xc0, 0xff, 0x07, 0xc0, 0xff, 
	0x77, 0xc0, 0xff, 0x7b, 0xc0, 0xff, 0x7b, 0xc0, 0xff, 0x7b, 0xc0, 0xff, 0x7b, 0xc0, 0xff, 0x7f, 
	0xc0, 0xff, 0x7f, 0xc0, 0xff, 0x7f, 0xc0, 0xf0, 0x7f, 0xc0, 0xe0, 0x7f, 0xc0, 0xe0, 0x7f, 0xc0, 
	0xe0, 0xff, 0xc0, 0xe1, 0xff, 0xc0
};
const unsigned char sixteenth[] = {
	0xff, 0xff, 0xc0, 0xff, 0xdf, 0xc0, 0xff, 0xcf, 0xc0, 0xff, 0xc7, 0xc0, 0xff, 0xc3, 0xc0, 0xff, 
	0xd9, 0xc0, 0xff, 0xcd, 0xc0, 0xff, 0xc5, 0xc0, 0xff, 0xc1, 0xc0, 0xff, 0xd9, 0xc0, 0xff, 0xdd, 
	0xc0, 0xff, 0xdd, 0xc0, 0xfe, 0x9d, 0xc0, 0xf8, 0x1d, 0xc0, 0xf0, 0x1f, 0xc0, 0xf0, 0x1f, 0xc0, 
	0xf0, 0x3f, 0xc0, 0xf8, 0xff, 0xc0
};

/* Typedefs ------------------------------------------------------------------*/
TIM_HandleTypeDef TimHandle;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim7;
I2C_HandleTypeDef hi2c1;
DAC_HandleTypeDef DacHandle;
ADC_HandleTypeDef hadc1;

/* Gloabl variables ----------------------------------------------------------*/
uint32_t uwPrescalerValue = 0;
// menu variables
volatile uint8_t option;
char buffer[20];
// value variables with default values
volatile double tempo = 80;
volatile double beat = 0;
volatile uint8_t subdivision = 0;
uint16_t volume = 1024;
// variables for rotary encoder
volatile uint32_t last_encoder_press = 0;
volatile uint32_t last_dt_interrupt_time = 0;
// variables for subdivisions
uint8_t subdivision_phase = 0;
volatile bool subdivision_tick_pending = false;
// variables for tap tempo 
volatile uint8_t tap_mode_active = 0;
volatile uint8_t tap_count = 0;
volatile uint32_t tap_timestamps[TAP_MAX_COUNT] = {0};
volatile uint32_t last_tap_time = 0;
static uint32_t last_tap_press_time = 0;
// variables for mute
volatile uint8_t metronome_muted = 0;
static uint32_t last_mute_press = 0;
// variables for low-power
volatile uint32_t last_interaction_time = 0;
volatile uint8_t in_low_power_run_mode = 0; 
// variables for audio output 
volatile bool dac_tick_pending = false;
volatile uint16_t tick_counter = 0;
// variables for volume
uint16_t scaled_volume = 5; // scale from 0 to 8

/* Function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void Error_Handler(void);
static void EXTI0_IRQHandler_Config(void);
static void EXTI9_5_IRQHandler_Config(void);
static void EXTI1_IRQHandler_Config(void);
static void EXTI15_10_IRQHandler_Config(void);
void setup_adc(void);
void GPIO_Init(void);
void TIM2_Init(void);
void MX_I2C_MspInit(void);
static void MX_I2C1_Init(void);
void DAC_Init(void);
void TIM7_Init(void);
void menu(void);
void draw_indicator(uint8_t x, uint8_t y, SSD1306_COLOR color);
void update_indicator(uint8_t option);
void change_values(int8_t change);
void update_menu(uint8_t change_option);
void update_metronome_timer(uint16_t tempo);
void show_tempo(void);
void show_beat(void);
void update_subdivision_timer(void);
void show_subdivision(int8_t subdivision);
void calculate_tap_tempo(void);
void show_volume_bar(uint16_t volume_reference);
void generate_dac_tick(const uint16_t *pulse_shape, uint8_t length, uint16_t volume_scale);
void disable_EXTIs(void);
void enable_EXTIs(void);
void mute(void);
void unmute(void);
void enter_sleep_mode(void);
void enter_low_power_sleep_mode(void);
void exit_low_power_run_mode(void);
void handle_sleep_mode(uint32_t current_time);
void handle_low_power_sleep_mode(uint32_t current_time);
void handle_tick(void);

int main(void)
{
 	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	
  HAL_Init();

  /* Configure the system clock to 80 MHz */
  SystemClock_Config();

  /* Configure LED3 */
  BSP_LED_Init(LED3);

	/* Configure peripherals */
	GPIO_Init(); 
	DAC_Init();
	MX_I2C_MspInit();
	MX_I2C1_Init();
	ssd1306_Init();
	
	/* Configure EXTIs */
	EXTI0_IRQHandler_Config();
	EXTI9_5_IRQHandler_Config();
	EXTI1_IRQHandler_Config();
	EXTI15_10_IRQHandler_Config();
	TIM2_Init();	
	TIM7_Init();
	
	/* Start menu with 80 Bpm*/
	menu();
	update_metronome_timer(80);
	
	while (1)
  {					
		uint32_t current_time = HAL_GetTick();
		
		if (!metronome_muted) 
		{	
			handle_sleep_mode(current_time);
			handle_tick();
		}
		else
			handle_low_power_sleep_mode(current_time);
		
		if (tap_mode_active && (current_time - last_tap_time > TAP_TIMEOUT_MS))
			calculate_tap_tempo();	
	}
}

static void Error_Handler(void)
{
  /* Turn LED3 on */
  BSP_LED_On(LED3);
  while (1)
  {
  }
}

void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* MSI is enabled after System reset, activate PLL with MSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLP = 7;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}

/* DAC setup ---------------------------------------------------------------------------------------*/
void DAC_Init(void) 
{
	__HAL_RCC_DAC1_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitDAC = {0};
    GPIO_InitDAC.Pin = GPIO_PIN_4;
    GPIO_InitDAC.Mode = GPIO_MODE_ANALOG;
    GPIO_InitDAC.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitDAC);

    DAC_ChannelConfTypeDef sConfigDAC = {0};
    DacHandle.Instance = DAC1;
    HAL_DAC_Init(&DacHandle);

    sConfigDAC.DAC_Trigger = DAC_TRIGGER_NONE;
    sConfigDAC.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    HAL_DAC_ConfigChannel(&DacHandle, &sConfigDAC, DAC_CHANNEL_1);

    HAL_DAC_Start(&DacHandle, DAC_CHANNEL_1);
}

/* GPIO setupt for encoder CLK pin---------------------------------------------------------------------*/
void GPIO_Init(void) 
{
	static GPIO_InitTypeDef  GPIO_InitCLK;
	
	GPIO_InitCLK.Mode  = GPIO_MODE_INPUT;
	GPIO_InitCLK.Pull  = GPIO_PULLUP;
  GPIO_InitCLK.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  GPIO_InitCLK.Pin = GPIO_PIN_9;
  HAL_GPIO_Init(GPIOA, &GPIO_InitCLK);
}

/* GPIO INTERRUPT FOR OPTION -------------------------------------------------------------------------*/
static void EXTI0_IRQHandler_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

void EXTI0_IRQHandler(void)
{
	last_interaction_time = HAL_GetTick();
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
	if (last_interaction_time - last_encoder_press < DEBOUNCE_MS) 
		return;
  last_encoder_press = last_interaction_time;
	
	if (option < 3) 
		option++;
	else 
		option = 0;
	
	update_indicator(option);
}

/* GPIO INTERRUPT FOR VALUE --------------------------------------------------------------------------*/
static void EXTI9_5_IRQHandler_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_8;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void EXTI9_5_IRQHandler(void)
{
  if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_8) != RESET) 
  {
		last_interaction_time = HAL_GetTick();
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
    if (last_interaction_time - last_dt_interrupt_time < ENCODER_DEBOUNCE_MS) 
			return;
		last_dt_interrupt_time = last_interaction_time;	
		
    GPIO_PinState clk_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9);
    if (clk_state == GPIO_PIN_RESET) // clockwise
			change_values(1);
    else // counter clockwise
			change_values(-1);
		update_menu(option);
  }
}

/* GPIO INTERRUPT FOR MUTE ---------------------------------------------------------------------------*/
static void EXTI1_IRQHandler_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void EXTI1_IRQHandler(void)
{
	uint32_t now = HAL_GetTick();		
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
	if (now - last_mute_press < DEBOUNCE_MS) 
		return;
	last_mute_press = now;
	
	metronome_muted = !metronome_muted;
	if (metronome_muted)
	{
		mute();	
		disable_EXTIs();
		show_volume_bar(0);
	}
	else
	{
		last_mute_press = 0;
		exit_low_power_run_mode(); 
		enable_EXTIs();
		unmute();
		if (!in_low_power_run_mode) 
			show_volume_bar(scaled_volume);		
	}
}

/* GPIO INTERRUPT FOR TAP TEMPO -----------------------------------------------------------------------*/
static void EXTI15_10_IRQHandler_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_11;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
	uint32_t now = HAL_GetTick();
	
	if (now - last_tap_press_time < DEBOUNCE_MS) 
		return;
	last_tap_press_time = now;
		
	// First tap or timeout = restart
  if (!tap_mode_active || now - last_tap_time > TAP_TIMEOUT_MS)
	{
		tap_mode_active = 1;
		tap_count = 0;
	}
	
	last_tap_time = now;
	
	if (tap_count < TAP_MAX_COUNT)
	{
		tap_timestamps[tap_count++] = now;
	}
	
	// Metronome stays muted while tapping
	if (!metronome_muted) {
		mute();
		disable_EXTIs();
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// All GPIO EXTI logic is handled on IRQHandlers
}

/* TIMER INTERRUPT FOR TEMPO --------------------------------------------------------------------------*/
void TIM2_Init(void) {
  htim2.Instance = TIM2;

  htim2.Init.Period            = 10000 - 1;
  htim2.Init.Prescaler         = 8000 - 1;
  htim2.Init.ClockDivision     = 0;
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.RepetitionCounter = 0;

  __HAL_RCC_TIM2_CLK_ENABLE();
  
  HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);

  HAL_NVIC_EnableIRQ(TIM2_IRQn);
	
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    /* Starting Error */
    Error_Handler();
  }
}

void TIM2_IRQHandler(void)
{
	uint32_t current_time = HAL_GetTick();
  HAL_TIM_IRQHandler(&htim2);
	tick_counter++;
	
	// Downbeat
	if (tick_counter >= beat)
		tick_counter = 0;
	
	// Metronome stays muted while interacting with encoder
	if (current_time - last_interaction_time > START_TICKING_TIMEOUT) 
		dac_tick_pending = true;
	else 
	{
		tick_counter = beat - 1;
		subdivision_phase = 0;
		__HAL_TIM_SET_COUNTER(&htim2, 0);
		__HAL_TIM_SET_COUNTER(&htim7, 0);
	}
	
	update_subdivision_timer();	
}

/* TIMER INTERRUPT FOR SUBDIVISIONS ------------------------------------------------------------------*/
void TIM7_Init(void) {
  htim7.Instance = TIM7;

  htim7.Init.Period            = 500 - 1;
  htim7.Init.Prescaler         = 8000 - 1;
  htim7.Init.ClockDivision     = 0;
  htim7.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim7.Init.RepetitionCounter = 0;

  __HAL_RCC_TIM7_CLK_ENABLE();
  
  HAL_NVIC_SetPriority(TIM7_IRQn, 4, 0);

  HAL_NVIC_EnableIRQ(TIM7_IRQn);
	
	if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  if (HAL_TIM_Base_Start_IT(&htim7) != HAL_OK)
  {
    /* Starting Error */
    Error_Handler();
  }
}

void TIM7_IRQHandler(void)
{
  uint32_t current_time = HAL_GetTick();
	HAL_TIM_IRQHandler(&htim7);
	
	// Metronome stays muted while interacting with encoder
	if (current_time - last_interaction_time > START_TICKING_TIMEOUT) 
		subdivision_tick_pending = true;
	else 
	{
		tick_counter = beat - 1;
		subdivision_phase = 0;
		__HAL_TIM_SET_COUNTER(&htim2, 0);
		__HAL_TIM_SET_COUNTER(&htim7, 0);
	}
	
	if (subdivision_phase > 0)
		subdivision_phase--;
	
	// 1 (eighths) or 2 (sixteenths) subdivision beats completed
	if (subdivision_phase == 0)
		HAL_TIM_Base_Stop_IT(&htim7);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// All TIM EXTI logic is handled in IRQHandlers
}

/* OLED setup ----------------------------------------------------------------------------------------*/
void MX_I2C_MspInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;
	
	__HAL_RCC_GPIOB_CLK_ENABLE();
  
    /* I2C1 GPIO Configuration    
    PB6 ------> I2C1_SCL
    PB7 ------> I2C1_SDA 
    */
	
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    __HAL_RCC_I2C1_CLK_ENABLE();
}

/* I2C1 init function */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0xE0330309;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

void menu(void) 
{
	// Show default values for bpm, beat and subdivisions
	#ifdef SSD1306_INCLUDE_FONT_16x24
    ssd1306_SetCursor(42, 3);
    ssd1306_WriteString("80", Font_16x24, White);
  #endif
	#ifdef SSD1306_INCLUDE_FONT_7x10
    ssd1306_SetCursor(81, 17);
    ssd1306_WriteString("bpm", Font_7x10, White);
  #endif
	#ifdef SSD1306_INCLUDE_FONT_11x18
    ssd1306_SetCursor(10, 43);
    ssd1306_WriteString("beat: ", Font_11x18, White);
	ssd1306_SetCursor(65, 43);
	ssd1306_WriteString("0", Font_11x18, White);
  #endif
	ssd1306_FillRectangle(100, 43, 117, 60, White);
	ssd1306_DrawBitmap(100, 43, quarter, 18, 18, Black);
	draw_indicator(24, 15, White);
	
	// Draw default volume bar
	ssd1306_FillRectangle(122, 0, 128, 64, White);
	ssd1306_FillRectangle(124, 1, 128, 63, Black);
	ssd1306_FillRectangle(124, 24, 128, 64, White);
	
	ssd1306_UpdateScreen();
}

void draw_indicator(uint8_t x, uint8_t y, SSD1306_COLOR color) 
{	
	for(int i = 0; i <= 3; i++)
		ssd1306_Line(x-6+i, y-3+i, x-6+i, y+3-i, color);
}

void update_indicator(uint8_t option) 
{
	ssd1306_FillRectangle(18, 12, 24, 18, Black);
	ssd1306_FillRectangle(2, 49, 8, 55, Black);
	ssd1306_FillRectangle(93, 49, 99, 55, Black);
	ssd1306_FillRectangle(114, 3, 120, 9, Black);
	switch(option) 
	{
		case 0:
			draw_indicator(24, 15, White);
			break;
		case 1:
			draw_indicator(8, 52, White);
			break;
		case 2:
			draw_indicator(99, 52, White);
			break;
		case 3:
			draw_indicator(120, 6, White);
		break;
	}
	ssd1306_UpdateScreen();
}

void change_values(int8_t change) 
{
	switch (option) 
	{
		case 0:
			{
				if (tempo > 30 && tempo < 240) tempo += change;
				else if (tempo ==  30 && change == 1) tempo += change;
				else if (beat == 240 && change == -1) beat += change;
				update_metronome_timer(tempo);
				break;
			}
		case 1:
			{
				if (beat > 0 && beat < 9) beat += change;
				else if (beat == 0 && change == 1) beat += change;
				else if (beat == 9 && change == -1) beat += change;
				break;
			}
		case 2:
			{
				if (subdivision > 0 && subdivision < 2) subdivision += change;
				else if (subdivision == 0 && change == 1) subdivision += change;
				else if (subdivision == 2 && change == -1) subdivision += change;
				break;
			}
		case 3:
			{
				if (scaled_volume > 0 && scaled_volume < 8) scaled_volume += change;
				else if (scaled_volume == 0 && change == 1) scaled_volume += change;
				else if (scaled_volume == 8 && change == -1) scaled_volume += change;
				volume = volume_lookup[scaled_volume];
				break;
			}
	}
}

void update_menu(uint8_t change_option) 
{
	switch (change_option) 
	{
		case 0:
			show_tempo();
			break;
		case 1:
			show_beat();
			break;
		case 2:
			show_subdivision(subdivision);
			break;
		case 3:
			show_volume_bar(scaled_volume);
			break;
	}
	ssd1306_UpdateScreen();
}

void update_metronome_timer(uint16_t tempo)
{
	uint32_t timer_freq = 10000; // 0.1 ms
	// Convert bpm to ms
	uint32_t period_value = (60 * timer_freq) / tempo;

	__HAL_TIM_SET_AUTORELOAD(&htim2, period_value);
	__HAL_TIM_SET_COUNTER(&htim2, 0);  
}

void show_tempo(void) 
{
	ssd1306_FillRectangle(26, 3, 74, 27, Black);
	snprintf(buffer, sizeof(tempo), "%.0f", tempo);
	#ifdef SSD1306_INCLUDE_FONT_16x24
		if (tempo < 100) 
			ssd1306_SetCursor(42, 3);
		else 
			ssd1306_SetCursor(26, 3);
		ssd1306_WriteString(buffer, Font_16x24, White);
	#endif
}

void show_beat(void) 
{
	ssd1306_FillRectangle(65, 43, 76, 61, Black);
	snprintf(buffer, sizeof(beat), "%.0f", beat);
	#ifdef SSD1306_INCLUDE_FONT_11x18
		ssd1306_SetCursor(65, 43);
		ssd1306_WriteString(buffer, Font_11x18, White);
	#endif
}

void update_subdivision_timer(void) 
{
	if (subdivision == 1)  // eighths
  {
		__HAL_TIM_SET_AUTORELOAD(&htim7, htim2.Init.Period / 2);  // middle of beat
		__HAL_TIM_SET_COUNTER(&htim7, 0);
		subdivision_phase = 1;
		HAL_TIM_Base_Start_IT(&htim7);
	}
	else if (subdivision == 2)  // sixteenths
	{
		__HAL_TIM_SET_AUTORELOAD(&htim7, htim2.Init.Period / 3);  // 2 interleaved beats
		__HAL_TIM_SET_COUNTER(&htim7, 0);
		subdivision_phase = 2;
		HAL_TIM_Base_Start_IT(&htim7);
	}
	else 
	{
		HAL_TIM_Base_Stop_IT(&htim7);
		subdivision_phase = 0;
	}
}

void show_subdivision(int8_t subdivision) 
{
	ssd1306_FillRectangle(100, 43, 117, 60, White);
	switch (subdivision) {
		case 0:
			ssd1306_DrawBitmap(100, 43, quarter, 18, 18, Black);
			break;
		case 1:
			ssd1306_DrawBitmap(100, 43, eighth, 18, 18, Black);
			break;
		case 2:
			ssd1306_DrawBitmap(100, 43, sixteenth, 18, 18, Black);
			break;
	}
	ssd1306_UpdateScreen();
}

void calculate_tap_tempo(void) {
	if (in_low_power_run_mode)
		exit_low_power_run_mode();
	
	tap_mode_active = 0;

	// Only calculate tempo if there were at least two taps
	if (tap_count >= 2)
	{
		uint32_t total_interval = 0;
		for (uint8_t i = 1; i < tap_count; i++)
			total_interval += tap_timestamps[i] - tap_timestamps[i - 1];

		uint32_t avg_interval = total_interval / (tap_count - 1);  
		// Convert ms to bpm
		uint16_t new_bpm = 60000 / avg_interval;

		if (new_bpm >= 30 && new_bpm <= 240)
		{
			tempo = new_bpm;
			update_metronome_timer(tempo); 
			update_menu(0);
		}
	}

	// Reset tap data
	tap_count = 0;

	// Resume metronome
	if (metronome_muted) 
	{
		enable_EXTIs();
		unmute();
		show_volume_bar(scaled_volume);
	}
}

void show_volume_bar(uint16_t volume_reference) 
{
	// Convert 0 to 8 volume scale to a 64 pixel bar
	uint8_t bar_size = volume_reference * 8;
	ssd1306_FillRectangle(124, 62, 128, 1, Black);
	ssd1306_FillRectangle(124, 64, 128, 64 - bar_size, White);
	ssd1306_UpdateScreen();
}

void generate_dac_tick(const uint16_t *pulse_shape, uint8_t length, uint16_t volume_scale)
{
	for (uint8_t i = 0; i < length; i++)
	{
		uint16_t scaled_value = (pulse_shape[i] * volume_scale) >> 12;

		if (scaled_value > DAC_RESOLUTION)
				scaled_value = DAC_RESOLUTION;

		HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_1, DAC_ALIGN_12B_R, scaled_value);
		for (volatile uint32_t j = 0; j < 400; j++) __NOP();
	}

	HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
}

void disable_EXTIs(void) 
{
	// Tap tampo EXTI (15_10) will remain active
	HAL_NVIC_DisableIRQ(EXTI0_IRQn);
	HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
}

void enable_EXTIs(void) 
{
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
	NVIC_ClearPendingIRQ(EXTI0_IRQn);
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_8);
	NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void mute(void) 
{
	metronome_muted = 1;
	HAL_TIM_Base_Stop_IT(&htim2);
	HAL_TIM_Base_Stop_IT(&htim7);
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_SET_COUNTER(&htim7, 0);
}

void unmute(void) 
{
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
  __HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);
	
	// Settings to re-start the metronome from the downbeat 
	metronome_muted = 0;
	tick_counter = beat - 1;
	subdivision_phase = 0;
	dac_tick_pending = 1;
	
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_SET_COUNTER(&htim7, 0);
	HAL_TIM_Base_Start_IT(&htim2);
}

void enter_sleep_mode(void) {
	if (HAL_IS_BIT_SET(PWR->SR2, PWR_SR2_REGLPF))
    {
        HAL_PWREx_DisableLowPowerRunMode(); 
    }

    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    HAL_ResumeTick();
}

void enter_low_power_sleep_mode(void) {	
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	HAL_StatusTypeDef ret; // Variable to check HAL function return status (error checking)

	// Configure Voltage Scaling to Range 2
	// Range 2 is typically used for lower frequencies and is a prerequisite for LPR
	__HAL_RCC_PWR_CLK_ENABLE(); 
	ret = HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2);
	if (ret != HAL_OK) {
    Error_Handler();
	}

	// Configure MSI to 2MHz and select it as System Clock 
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5; // MSI Range 5 is 2MHz 
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; // stop PLL from driving SYSCLK
																									 
																									 
	ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
	if (ret != HAL_OK) {
    Error_Handler();
	}

	// Select MSI as the system clock source.
	// AHB, APB1, APB2 prescalers are set to DIV1 (also 2 MHz)
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
																RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI; // Select MSI as SYSCLK
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	// Set Flash Latency, for 2MHz SYSCLK, 0 wait states is appropriate
	ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
	if (ret != HAL_OK) {
    Error_Handler();
	}
	
	// Update SystemCoreClock
	SystemCoreClockUpdate();
	
	// Enter low-power sleep mode
	HAL_SuspendTick();
	HAL_PWREx_EnableLowPowerRunMode(); 
	HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	HAL_ResumeTick();
	
  // MCU wakes up to low-power run mode after an EXTI during low-power sleep mode
	in_low_power_run_mode = 1;
}	

void exit_low_power_run_mode(void) {
	HAL_PWREx_DisableLowPowerRunMode();
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
		Error_Handler();
  }
  SystemClock_Config(); // Restore full 80MHz PLL configuration
	
  in_low_power_run_mode = 0;	
}

void handle_sleep_mode(uint32_t current_time) {
	if(in_low_power_run_mode) 
	{
		exit_low_power_run_mode();
		show_volume_bar(scaled_volume);
	}
	
	if (!tap_mode_active && current_time - last_interaction_time > UI_TIMEOUT_MS && current_time - last_tap_press_time > UI_TIMEOUT_MS) 
		enter_sleep_mode();
}

void handle_low_power_sleep_mode(uint32_t current_time) {
	if (!tap_mode_active && current_time - last_mute_press > UI_TIMEOUT_MS) 
	{
		if(!in_low_power_run_mode) 
			enter_low_power_sleep_mode();
	}
}

void handle_tick(void) 
{
	if (dac_tick_pending) 
	{
		dac_tick_pending = false;
		if (tick_counter == 0 && beat > 0)
			generate_dac_tick(downbeat_pulse, sizeof(downbeat_pulse)/sizeof(uint16_t), volume);
		else
			generate_dac_tick(regular_pulse, sizeof(regular_pulse)/sizeof(uint16_t), volume);
	}
	
	if (subdivision_tick_pending) 
	{
		subdivision_tick_pending = false;
		generate_dac_tick(subdivision_pulse, sizeof(subdivision_pulse)/sizeof(uint16_t), volume);
	}
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}

#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
