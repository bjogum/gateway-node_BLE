#ifndef LED_PWM_H
#define LED_PWM_H

void bl_pwm_init(void);
void set_bl_brightness(int percentage);
void buzzer_init();
void buzzer_on_fire();
void buzzer_on_intrusion();
void buzzer_off();

#endif
