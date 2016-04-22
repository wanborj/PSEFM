/*
 * $Id: mainloop.c,v 1.3 2008/10/22 19:41:18 casse Exp $
 *  
 * Copyright (C) 2003  Pascal Brisset, Antoine Drouin
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 *
 */
#include <avr/interrupt.h>
#include </home/wanbo/program/freertos-plus/freertos/stm32_p103.h>
#include "std.h"

#include "timer_auto.h"
#include "modem.h"
#include "adc.h"
#include "airframe.h"
#include "autopilot.h"
#include "spi_auto.h"
#include "link_fbw.h"
#include "gps.h"
#include "nav.h"
#include "infrared.h"
#include "estimator.h"
#include "downlink.h"

#ifndef PAPABENCH_SINGLE
	void fbw_init(void);
	void fbw_schedule(void);
#endif

/*Added by SunnyBeike*/
#ifdef PAPABENCH_SINGLE
	uint8_t _20Hz   = 0;
	uint8_t _1Hz   = 0;
#else
	static uint8_t _20Hz   = 0;
	static uint8_t _1Hz   = 0;
#endif
/*End Sunny*/

int main( void ) 
{
  uint8_t init_cpt;
  /* init peripherals */
  timer_init(); 
  modem_init();
  adc_init();
#ifdef CTL_BRD_V1_1  
  adc_buf_channel(ADC_CHANNEL_BAT, &buf_bat);
#endif
  spi_init();
  link_fbw_init();
  gps_init();
  nav_init();
  ir_init();
  estimator_init();
#	ifdef PAPABENCH_SINGLE
		fbw_init();
#	endif
	
	vPrintString("Init done \n\r");
  /* start interrupt task */
  //sei(); /*Fadia*/

  /* Wait 0.5s (for modem init ?) */
  init_cpt = 30;
  while (init_cpt) {
    if (timer_periodic())
      init_cpt--;
  }

	vPrintString("timeout \n\r");
  /*  enter mainloop */
  while( 1 ) {

	/*Added by SunnyBeike*/
	  _20Hz++;
	  _1Hz++;
	/*End Sunny*/

    if(timer_periodic()) {
      periodic_task();
	vPrintString("periodic_task end \n\r");
#		if PAPABENCH_SINGLE
			fbw_schedule();
			vPrintString("fbw_schedule end \n\r");
#		endif
	}
	//added by SunnyBeike
	gps_msg_received = 1;
	link_fbw_receive_complete = 1;

    if (gps_msg_received) 
    {
	/*receive_gps_data_task()*/
	vPrintString("T_9 receive_gps_data_task start! \n\r"); //SunnyBeike
	parse_gps_msg();
	send_gps_pos();
        send_radIR();
        send_takeOff();
	vPrintString("T_9 receive_gps_data_task end! \n\r"); //SunnyBeike

    }
    if (link_fbw_receive_complete) {
      link_fbw_receive_complete = FALSE;
      radio_control_task();
			vPrintString("radio_control_task end \n\r");
    }
	/*Added by SunnyBeike*/
	  if (_20Hz>=3) _20Hz=0;
	  if (_1Hz>=61) _1Hz=0;
	/*End SunnyBeike*/

  } 
  return 0;
}
