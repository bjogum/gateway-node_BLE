#include "driver/ledc.h"

// DISPLAY BACKGROUND LIGHT
#define LCD_BL_GPIO           3
#define LEDC_LS_TIMER         LEDC_TIMER_0
#define LEDC_LS_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH0_GPIO      LCD_BL_GPIO
#define LEDC_LS_CH0_CHANNEL   LEDC_CHANNEL_0

// ALARM BUZZER
#define BUZZER_GPIO           47
#define BUZZER_CHANNEL        LEDC_CHANNEL_1
#define BUZZER_TIMER          LEDC_TIMER_1
#define BUZZER_MODE           LEDC_LOW_SPEED_MODE

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



void buzzer_init(){
  // skapar timer config för summern
  ledc_timer_config_t buzzerTimer = {
    .speed_mode       = BUZZER_MODE,
    .timer_num        = BUZZER_TIMER,
    .duty_resolution  = LEDC_TIMER_13_BIT,
    .freq_hz          = 2000,
    .clk_cfg          = LEDC_AUTO_CLK
  };
  ledc_timer_config(&buzzerTimer);

  // skapar kanal config för summern
  ledc_channel_config_t buzzerChannel = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .channel          = BUZZER_CHANNEL,
    .timer_sel        = BUZZER_TIMER,
    .gpio_num         = BUZZER_GPIO,
    .duty             = 0,
    .hpoint           = 0
  };
  ledc_channel_config(&buzzerChannel);

}

// skrik
void buzzer_on_fire() {
    // 1. Sätt duty cycle till 50% (4095)
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 3000);
    
    // 2. Säg till hårdvaran att verkställa ändringen
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

// skrik
void buzzer_on_intrusion() {
    // 1. Sätt duty cycle till 50% (4095)
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 3000);
    
    // 2. Säg till hårdvaran att verkställa ändringen
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

// tyst
void buzzer_off() {
    // 1. Sätt duty cycle till 0
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    
    // 2. Säg till hårdvaran att verkställa ändringen
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}





