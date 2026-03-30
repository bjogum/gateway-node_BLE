#include "driver/ledc.h"

#define LCD_BL_GPIO       3
#define LEDC_LS_TIMER     LEDC_TIMER_0
#define LEDC_LS_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH0_GPIO     LCD_BL_GPIO
#define LEDC_LS_CH0_CHANNEL  LEDC_CHANNEL_0

void bl_pwm_init(void){
  ledc_timer_config_t ledc_timer={
    .speed_mode = LEDC_LS_MODE,
    .timer_num = LEDC_LS_TIMER,
    .duty_resolution = LEDC_TIMER_10_BIT, //0 to 1023
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK
  }; ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel={
    .speed_mode = LEDC_LS_MODE,
    .channel = LEDC_LS_CH0_CHANNEL,
    .timer_sel = LEDC_LS_TIMER,
    .intr_type = LEDC_INTR_DISABLE,
    .gpio_num = LEDC_LS_CH0_GPIO,
    .duty = 1023, //100% brightnes
    .hpoint  = 0
  };ledc_channel_config(&ledc_channel);
}

void set_bl_brightness(int percentage){
  if (percentage > 100) percentage=100;
  if(percentage < 0) percentage=0;

  uint32_t duty = (percentage * 1023)/100;
  
  ledc_set_duty(LEDC_LS_MODE, LEDC_LS_CH0_CHANNEL, duty);
  ledc_update_duty(LEDC_LS_MODE, LEDC_LS_CH0_CHANNEL);
}







