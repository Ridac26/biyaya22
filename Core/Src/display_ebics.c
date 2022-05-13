/*
 * display_ebics.c
 *
 *  Created on: 12.11.2020
 *      Author: Gaswerke
 */

#include "main.h"
#include "config.h"
#include "stm32f1xx_hal.h"
#include "print.h"
#include "packet.h"
#include "VescCommand.h"

UART_HandleTypeDef huart3;
uint8_t ui8_rx_buffer[64];
uint8_t ui8_tx_buffer[12];



enum { STATE_LOST, STATE_PAYLOAD };
void comm_uart_send_packet(unsigned char *data, unsigned int len);
void putbuffer(unsigned char *buf, unsigned int len);
void comm_uart_send_packet(unsigned char *data, unsigned int len);
void checkUART_rx_Buffer(uint8_t UART_HANDLE);



static int tail=0;
static uint8_t ui8_pkt[12];
static int state = STATE_LOST; 
static int pkt_pos=0;
static int chkSum = 0; 


void ebics_init() {
//        CLEAR_BIT(huart3.Instance->CR3, USART_CR3_EIE);
	if (HAL_UART_Receive_DMA(&huart3, (uint8_t*) ui8_rx_buffer, sizeof(ui8_rx_buffer)) != HAL_OK) {
		Error_Handler();
	}


}

void ebics_reset() {
	if (HAL_UART_Receive_DMA(&huart3, (uint8_t*) ui8_rx_buffer, sizeof(ui8_rx_buffer)) != HAL_OK) {
		Error_Handler();
	}
        tail=0;
}



//_______________________________________________________________________________________________________________
// UART functions for VESC-Tool
void putbuffer(unsigned char *buf, unsigned int len){
	HAL_UART_Transmit_DMA(&huart3, (uint8_t *)buf, len);
}

void process_packet(unsigned char *data, unsigned int len){

	commands_process_packet(data, len, &comm_uart_send_packet);

}

void comm_uart_send_packet(unsigned char *data, unsigned int len) {
	packet_send_packet(data, len, 0); //UART_HANDLE
}

void checkUART_rx_Buffer(uint8_t UART_HANDLE){
	static uint8_t last_pointer_position=64;
	static uint8_t recent_pointer_position;
	recent_pointer_position = DMA1_Channel3->CNDTR;
	while(last_pointer_position != (recent_pointer_position-1)){
		packet_process_byte(ui8_rx_buffer[last_pointer_position], UART_HANDLE);
		last_pointer_position=(last_pointer_position+1) % sizeof(ui8_rx_buffer);
	}

}

void process_ant_page_one(uint8_t *ui8_pkt, MotorState_t *MS, MotorParams_t *MP){
	switch (ui8_pkt[3]) {
	case 16: {
		/*
		 message[4] = State.Wheel_Circumference & 0xFF; //Low Byte
		 message[5] = State.Wheel_Circumference >> 8 & 0xFF; // HiByte
		 message[6] = State.Travel_Mode_State;
		 message[7] = State.Display_Command & 0xFF; //Low Byte
		 message[8] = State.Display_Command >> 8 & 0xFF; // HiByte
		 message[9] = State.Manufacturer_ID & 0xFF; //Low Byte
		 message[10] = State.Manufacturer_ID >> 8 & 0xFF; //Hi Byte
		 */
		MP->wheel_cirumference = ui8_pkt[5] << 8 | ui8_pkt[4];
		MS->regen_level = ui8_pkt[6] & 0x07;
		MS->assist_level = ui8_pkt[6] >> 3 & 0x07;

	} // end case 16
		break;

	case 6: {
		if (ui8_pkt[4])
			autodetect();
#ifndef ADCTHROTTLE
		MS->i_q_setpoint = (int16_t) (ui8_pkt[8] << 8 | ui8_pkt[7]);
#endif
	}
		break;

	default: {
		MS->i_q_setpoint = 0; // stop motor for safety reason
	}
	} //end switch
	  //}// end if chkSum
}	 //end process_ant_page

void process_ant_page(MotorState_t *MS, MotorParams_t *MP) {
        int head = 64 - DMA1_Channel3->CNDTR;

        while(tail != head) { //Consume available bytes
                switch(state){
                        case STATE_LOST: {
                                if(ui8_rx_buffer[tail] == 0xAA){
                                        pkt_pos=1;
                                        ui8_pkt[0] = ui8_rx_buffer[tail];
                                        chkSum = 0xAA;
                                        state = STATE_PAYLOAD;
                                }
                                }break;
                        case STATE_PAYLOAD: {
                                        ui8_pkt[pkt_pos++] = ui8_rx_buffer[tail];
                                        chkSum ^= ui8_rx_buffer[tail];
                                        if(pkt_pos==12){
                                                if(chkSum == 0){ //CRC was good
                                                        process_ant_page_one(ui8_pkt,MS,MP);
                                                }
                                                state = STATE_LOST;
                                        }
                                }break;
                }
                tail = (tail + 1) % sizeof(ui8_rx_buffer);
        }
}


void send_ant_page(uint8_t page, MotorState_t *MS, MotorParams_t *MP) {

	switch (page) {
	case 1: {
		/*
		 State.Temperature_State=RxAnt[4];
		 State.Travel_Mode_State=RxAnt[5];
		 State.System_State=RxAnt[6];
		 State.Gear_State=RxAnt[7];
		 State.LEV_Error=RxAnt[8];
		 State.Speed=RxAnt[10]<<8|RxAnt[9];
		 */
		uint8_t temperature_state = 1; //to do: set Temperature state Byte according to ANT+LEV
		uint16_t speedx10 = MP->wheel_cirumference
				/ ((MS->Speed * MP->pulses_per_revolution) >> 3) * 36; // *3,6 for km/h then *10 for LEV standard definition.
		speedx10 = 250;
		ui8_tx_buffer[0] = 164; //Sync binary 10100100;
		ui8_tx_buffer[1] = 12;  //MsgLength
		ui8_tx_buffer[2] = 0x4E;  // MsgID for 0x4E for "broadcast Data"
		ui8_tx_buffer[3] = page;

		ui8_tx_buffer[4] = temperature_state;
		ui8_tx_buffer[5] = MS->regen_level | MS->assist_level << 3;
		ui8_tx_buffer[6] = MS->system_state;
		ui8_tx_buffer[7] = MS->gear_state;
		ui8_tx_buffer[8] = MS->error_state;
		ui8_tx_buffer[9] = speedx10 & 0xFF; //low byte of speed
		ui8_tx_buffer[10] = speedx10 >> 8 & 0x07; // lower 3 Bytes of high byte

		int chkSum = 0;

		for (uint8_t i = 0; i < 11; i++) {
			chkSum ^= ui8_tx_buffer[i];
		}
		ui8_tx_buffer[11] = chkSum;

		HAL_UART_Transmit_DMA(&huart3, (uint8_t*) &ui8_tx_buffer, 12);

	} //end case 1
		break;

	default: {
		//do nothing
	}
	}	//end switch

} //end send page
