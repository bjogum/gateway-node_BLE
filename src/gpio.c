#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "alarm.h"

// DISPLAY BACKGROUND LIGHT
#define DISP_LED_GPIO         3
#define DISP_LED_TIMER        LEDC_TIMER_0
#define DISP_LED_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH0_GPIO      LCD_BL_GPIO
#define DISP_LED_CHANNEL      LEDC_CHANNEL_0

// ALARM BUZZER
#define BUZZER_GPIO           47
#define BUZZER_CHANNEL        LEDC_CHANNEL_1
#define BUZZER_TIMER          LEDC_TIMER_1
#define BUZZER_MODE           LEDC_LOW_SPEED_MODE

void bl_pwm_init(void){
  ledc_timer_config_t ledc_timer={
    .speed_mode = DISP_LED_MODE,
    .timer_num = DISP_LED_TIMER,
    .duty_resolution = LEDC_TIMER_10_BIT, //0 to 1023
    .freq_hz = 20000,
    .clk_cfg = LEDC_AUTO_CLK
  }; ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel={
    .speed_mode = DISP_LED_MODE,
    .channel = DISP_LED_CHANNEL,
    .timer_sel = DISP_LED_TIMER,
    .intr_type = LEDC_INTR_DISABLE,
    .gpio_num = DISP_LED_GPIO,
    .duty = 1023, //100% brightnes
    .hpoint  = 0
  };ledc_channel_config(&ledc_channel);
}

void set_bl_brightness(int percentage){
  if (percentage > 100) percentage=100;
  if(percentage < 0) percentage=0;

  uint32_t duty = (percentage * 1023)/100;
  
  ledc_set_duty(DISP_LED_MODE, DISP_LED_CHANNEL, duty);
  ledc_update_duty(DISP_LED_MODE, DISP_LED_CHANNEL);
}



void buzzer_init(){
  // skapar timer config för summern
  ledc_timer_config_t buzzerTimer = {
    .speed_mode       = BUZZER_MODE,
    .timer_num        = BUZZER_TIMER,
    .duty_resolution  = LEDC_TIMER_13_BIT,
    .freq_hz          = 3000,
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
  node.buzzer = true;

  // sätter freqvensen (tonhöjden)
  ledc_set_freq(BUZZER_MODE, BUZZER_CHANNEL, 2000);

  // sätter "volymen"
  ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 1000); //4096 standard
  ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);

}


// skrik
void buzzer_on_intrusion() {
  node.buzzer = true;
  
  while (1) {
      // Svep uppåt (från 600Hz till 1200Hz)
      for (int freq = 600; freq < 1200; freq += 10) {
          ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, freq);
          vTaskDelay(pdMS_TO_TICKS(10)); // hastigheten på svepet
      }

      // Svep nedåt (från 1200Hz till 600Hz)
      for (int freq = 1200; freq > 600; freq -= 10) {
          ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, freq);
          vTaskDelay(pdMS_TO_TICKS(10)); // // hastigheten på svepet
      }
  }

  
}

// tyst
void buzzer_off() {
    // 1. Sätt duty cycle till 0
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    
    // 2. Säg till hårdvaran att verkställa ändringen
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);

    node.buzzer = false;
}





