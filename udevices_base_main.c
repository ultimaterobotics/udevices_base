
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "leds.h"
#include "urf_star_protocol.h"
#include "urf_radio.h"
#include "urf_timer.h"
#include "urf_uart.h"

uint8_t pin_led1 = 26;
uint8_t pin_led2 = 27;
uint8_t pin_led3 = 28;

uint8_t pin_tx = 5;
uint8_t pin_rx = 4;

uint8_t old_bootloader = 0;

int data_packet_len = 64;
int sync_packet_len = 64;
uint8_t sync_packet[260];
uint8_t data_packet[260];

uint16_t cycle_id = 0;

uint8_t cycle_packet[260];

uint32_t last_uart_pos = 0;

uint32_t uart_buf_length;

int pack_length = 0;
int pack_state = 0;
int pack_begin = 0;

uint8_t ble_mode = 0;
uint8_t ble_timed = 0;

uint32_t last_pos_rep_time = 0;

uint32_t last_uart_rf_pack = 0;

int bootloader_mode = 0;

void check_uart_packet()
{
	int pos = last_uart_pos;
	int upos = uart_get_rx_position();
	uint32_t ms = millis();
	if(ms - last_pos_rep_time > 500)
	{
//		uprintf("%lu: pos %d upos %d\n", ms, pos, upos);
		last_pos_rep_time = ms;
	}
	if(pos == upos) return;
//	uprintf("%lu: pos %d upos %d\n", ms, pos, upos);
	last_uart_pos = upos;
	
	int unproc_length = 0;
	unproc_length = upos - pos;
	if(unproc_length < 0) unproc_length += uart_buf_length;
	uint8_t *ubuf = uart_get_rx_buf();
	
//	uint8_t msg_start[16];
//	for(int x = 0; x < 16; x++)
//		msg_start[x] = ubuf[pos+x];
//	uart_send(msg_start, 16);
	
	for(uint8_t pp = pos; pp != upos; pp++)
	{
		if(pp >= uart_buf_length) pp = 0;
		if(pack_state == 0 && ubuf[pp] == 29)
		{
			pack_state = 1;
			continue;
		}
		if(pack_state == 1)
		{
			if(ubuf[pp] == 115) pack_state = 2;
			else pack_state = 0;
			continue;
		}
		if(pack_state == 2)
		{
			pack_length = ubuf[pp];
			if(pack_length >= uart_buf_length-2) //something is wrong or too long packet
			{
				pack_length = 0;
				pack_state = 0;
				continue;
			}
			pack_begin = pp+1;
			if(pack_begin >= uart_buf_length) pack_begin = 0;
			pack_state = 3;
			continue;
		}
		if(pack_state == 3)
		{
			int len = pp - pack_begin;
			if(len < 0) len += uart_buf_length;
			if(len >= pack_length)
			{
				int bg = pack_begin;
				sync_packet[0] = 10;
				sync_packet[1] = pack_length+2;
				for(int x = 0; x < pack_length; x++)
				{
					sync_packet[2+x] = ubuf[bg++];
					if(bg >= uart_buf_length) bg = 0;
				}
				int cmd_packet = 0;
				if(0)if(sync_packet[0] == 177 && sync_packet[1] == 103 && sync_packet[2] == 39)
				{
					cmd_packet = 1;
//					fr_ble_mode(37);
//					fr_listen();
					ble_mode = 1;
					ble_timed = 0;
				}
				if(0)if(sync_packet[0] == 177 && sync_packet[1] == 103 && (sync_packet[2] >= 40 && sync_packet[2] <= 42))
				{
					cmd_packet = 1;
//					fr_disable();
//					if(sync_packet[2] == 40) fr_ble_mode(37);
//					if(sync_packet[2] == 41) fr_ble_mode(38);
//					if(sync_packet[2] == 42) fr_ble_mode(39);
//					fr_listen();
					ble_mode = 1;
					ble_timed = 0;
				}
				if(0)if(sync_packet[0] == 177 && sync_packet[1] == 104 && (sync_packet[2] >= 40 && sync_packet[2] <= 42))
				{
					cmd_packet = 1;
//					fr_disable();
//					if(sync_packet[2] == 40) fr_ble_mode(37);
//					if(sync_packet[2] == 41) fr_ble_mode(38);
//					if(sync_packet[2] == 42) fr_ble_mode(39);
//					fr_listen();
					ble_mode = 1;
					ble_timed = 1;
				}
				if(0)if(sync_packet[0] == 103 && sync_packet[1] == 177 && sync_packet[2] == 39)
				{
					cmd_packet = 1;
//					fr_init(data_packet_len);
//					fr_listen();
					ble_mode = 0;
				}

				if(!cmd_packet)
				{
					if(!bootloader_mode) for(int x = 0; x < pack_length+2-4; x++)
					{
						if(sync_packet[x+1] == 0xFC && sync_packet[x+2] == 0xA3 && sync_packet[x+3] == 0x05)
						{
							bootloader_mode = 1;
//							fr_init(64);
							rf_init(21, 1000, 3); //not compatible with old versions
							delay_ms(2);
//							rf_init_ext(21, 250, 1, 0, 8, 40, 48);
	//						uprintf("entering bootloader mode\n");
						}
					}
					
					last_uart_rf_pack = millis();
					if(bootloader_mode)
					{
						while(rf_is_busy()) ;
						rf_send_and_listen(sync_packet, pack_length+2);
						leds_set_pwm(0, 255*((millis()%500)<250));
					}
					else star_queue_send(sync_packet, pack_length);
//					uprintf("pack sent %d %d\n", last_uart_pos, upos);
				}
				else
					uart_send(sync_packet, pack_length+2);
				pack_state = 0;
			}
		}
	}
	last_uart_pos = upos;
}

void fast_clock_start()
{
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_HFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {}
}
void slow_clock_start()
{
	NRF_CLOCK->LFCLKSRC = 0;
	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {}
}
void fast_clock_stop()
{
	slow_clock_start();
	NRF_CLOCK->TASKS_HFCLKSTOP = 1;
}

typedef struct sNode
{
	uint32_t id;
	uint32_t rx_time;
	uint8_t last_pack_id;
}sNode;

int main(void)
{
	NRF_UICR->NFCPINS = 0;
	fast_clock_start();
	
	leds_init();

	leds_set_pwm(0, 255);
	leds_set_pwm(1, 255);
	for(int x = 0; x < 3; x++)
	{
		leds_set_pwm(2, 255);
		delay_ms(200);
		leds_set_pwm(2, 0);
		delay_ms(200);
	}
	
	leds_set_pwm(0, 220);
	leds_set_pwm(1, 0);
	leds_set_pwm(2, 200);
    // Configure LED-pins as outputs


	uart_buf_length = uart_get_rx_buf_length();

	time_start();
	delay_ms(1000);

//	leds_set_pwm(1, 255);
//	leds_set_pwm(0, 255);

	star_init(21, 1000, 3200, 1);

	uint16_t sync_pack_id = 0;
	
//	uart_init(20, 19);
	uart_init(pin_tx, pin_rx, 921600);
	
//	uprintf("ultimate base running\n");
	for(int n = 0; n < 50; n++)
		cycle_packet[n] = 0;
//	cycle_packet[17] = 79;
//	cycle_packet[18] = 213;

	uint8_t sync_sent = 0;
	int data_count = 0;
	uint32_t last_rx_packet_time;
	uint32_t last_sync_time = 0;
	uint32_t last_sync_ms = 0;
	
/*	while(1)
	{
		int slen = sprintf(cycle_packet, "test mode %d\n", millis());
		uart_send(cycle_packet, slen);
		delay_ms(999);
	}*/

	int uart_send_pending = 0;
	int pending_length = 0;
	
	int multi_unit_mode = 0;
	int max_nodes = 16;
	int cycle_elem_time = 10;
	int active_nodes = 0;
	int last_phase_sent = -1;
	uint8_t cycle_state_pack[128];
	sNode nodes[32];
	uint32_t node_added = 0;
	nodes[0].id = 0x11223344; //not zeros to prevent false sync, nodes[0] is used as stub before first device is found
	
	leds_set_pwm(2, 220);
//	uprintf("ultimate base: all ready at %lu\n", millis());
	uint32_t last_print_ms = 0;

	NRF_WDT->CRV = 32767; //1 second timeout
	NRF_WDT->TASKS_START = 1;
		
    while (1)
    {
		NRF_WDT->RR[0] = 0x6E524635; //reload watchdog
		uint32_t ms = millis();
		
//		if(ble_mode)
//		{
//		}
//		else

		if(bootloader_mode == 0)
			star_loop_step();
		else
		{
			if(rf_has_new_packet())
			{
				int pack_len = rf_get_packet(data_packet);

				last_rx_packet_time = ms;
				data_count++;
				uint8_t packet_id = data_packet[0];
				uint8_t message_length = data_packet[1];
				uint8_t rssi_state = NRF_RADIO->RSSISAMPLE;
				
				if(old_bootloader) //there was a major mistake in it:
				{
					packet_id = data_packet[1];
					message_length = data_packet[0];
				}

				cycle_packet[0] = 79;
				cycle_packet[1] = 213;
				cycle_packet[2] = rssi_state;

				for(int x = 0; x < message_length; x++)
					cycle_packet[3+x] = data_packet[x];
				if(uart_send_remains() == 0)
					uart_send(cycle_packet, 3+message_length);			
			}
		}
		
		if(ms - last_uart_rf_pack > 3000 && bootloader_mode == 1)
		{
			bootloader_mode = 0;
			star_init(21, 1000, 3200, 1);
		}
		
		if(star_has_packet() && !bootloader_mode)
		{
			int pack_len = star_get_packet(data_packet, 256);

			last_rx_packet_time = ms;
			data_count++;
			uint8_t packet_id = data_packet[0];
			uint8_t message_length = data_packet[1];
			uint8_t rssi_state = NRF_RADIO->RSSISAMPLE;
			
			cycle_packet[0] = 79;
			cycle_packet[1] = 213;
			cycle_packet[2] = rssi_state;

			for(int x = 0; x < message_length; x++)
				cycle_packet[3+x] = data_packet[x];
			if(uart_send_remains() == 0)
				uart_send(cycle_packet, 3+message_length);			
		}
		if(0)
		{
			uint8_t dbg_pack[256];
			int dbg_len = rf_get_packet(dbg_pack);
			if(ms - last_print_ms > 500)
			{
				last_print_ms = ms;
				uprintf("%d - %d %d %02X %02X %02X %02X %02X %02X %02X %02X\n", dbg_len, dbg_pack[0], dbg_pack[1], dbg_pack[2], dbg_pack[3], dbg_pack[4], dbg_pack[5], dbg_pack[6], dbg_pack[7], dbg_pack[8], dbg_pack[9]);
			}
		}

		check_uart_packet();
		if(1)
		{
			int dc5 = data_count%510;
			int v = dc5 - 255;
			if(v < 0) v = -v;
			if(ble_mode)
			{
				leds_set_pwm(2, v);
				leds_set_pwm(1, 220);
			}
			else
			{
				leds_set_pwm(1, v);
				leds_set_pwm(2, 220);
			}
		}
			
    }
}