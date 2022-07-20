#include "leds.h"

uint8_t leds_pwm[3];


int led_pwm_vals[3] = {0, 512, 1023};
uint16_t pwm_seq[8];

void start_leds_pwm(int ms_length)
{
	if(NRF_PWM0->ENABLE)
	{
		NRF_PWM0->ENABLE = 0;
	}
	NRF_PWM0->PSEL.OUT[3] = 0xFFFFFFFF;
	for(int x = 0; x < 3; x++)
	{
		NRF_PWM0->PSEL.OUT[x] = led_pins[x];
		pwm_seq[x] = led_pwm_vals[x];
		pwm_seq[4+x] = 1023;
	}
	pwm_seq[3] = 0;
	pwm_seq[7] = 0;
	NRF_PWM0->ENABLE = 1;
	NRF_PWM0->MODE = 0;
	NRF_PWM0->COUNTERTOP = 1024;
	NRF_PWM0->PRESCALER = 0;
	NRF_PWM0->DECODER = 2;
	NRF_PWM0->LOOP = 0;
	NRF_PWM0->SEQ[0].PTR = (uint32_t)pwm_seq;
	NRF_PWM0->SEQ[0].CNT = 8;
	NRF_PWM0->SEQ[0].REFRESH = 16*ms_length;
	NRF_PWM0->INTEN = 1<<4; //SEQ0 END
//	if(ms_length == 0) NRF_PWM0->INTEN = 0;
	NRF_PWM0->TASKS_SEQSTART[0] = 1;
	NRF_PWM0->SHORTS = 1; //SEQ0END -> STOP
//	if(ms_length == 0) NRF_PWM0->SHORTS = 0;
}

void PWM0_IRQHandler()
{
	if(NRF_PWM0->EVENTS_SEQEND[0])
	{
		NRF_PWM0->ENABLE = 0;
		NRF_GPIO->OUTCLR = led_pin_mask[0];
		NRF_PWM0->EVENTS_SEQEND[0] = 0;
	}
}

void leds_start_pwm_timer()
{
	NRF_TIMER0->CC[0] = 8;
	NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
	NRF_TIMER0->BITMODE = 1; //8-bit
//	NRF_TIMER2->PRESCALER = 4; //16x prescaler
	NRF_TIMER0->PRESCALER = 7;

	NRF_TIMER0->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos); 
	NRF_TIMER0->SHORTS = 0;
//	NRF_TIMER2->SHORTS = 1; //COMPARE[0] -> CLEAR
	
	NVIC_EnableIRQ(TIMER0_IRQn);
	NRF_TIMER0->TASKS_START = 1;
}
void leds_pause_pwm()
{
	NVIC_DisableIRQ(TIMER0_IRQn);
}
void leds_resume_pwm()
{
	NVIC_EnableIRQ(TIMER0_IRQn);
}

uint8_t pwm_counter = 0;

void TIMER0_IRQHandler(void)
{
	NRF_TIMER0->EVENTS_COMPARE[0] = 0;
	NRF_TIMER0->TASKS_CLEAR = 1;
	pwm_counter += 2;
	
	for(int x = 0; x < 3; x++)
		if(pwm_counter < leds_pwm[x])
			NRF_GPIO->OUTSET = led_pin_mask[x];
		else
			NRF_GPIO->OUTCLR = led_pin_mask[x];
}

void leds_stop_pwm_timer()
{
	NRF_TIMER0->TASKS_SHUTDOWN = 1;
}
void leds_powerup_pwm_timer()
{
	NRF_TIMER0->TASKS_START = 1;
}

void leds_init()
{
	uint8_t lp[8] = {26, 27, 28};
	
	for(int x = 0; x < 3; x++)
	{
		led_pins[x] = lp[x];
		led_pin_mask[x] = 1<<led_pins[x];
		NRF_GPIO->DIRSET = 1<<led_pins[x];
	}	
	leds_start_pwm_timer();
}

void leds_set_pwm(uint8_t led, uint8_t value)
{
	if(led > 2) return;
	int v = value;
	v *= v;
	v >>= 8;
	leds_pwm[led] = v;
}
