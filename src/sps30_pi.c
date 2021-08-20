/*
 * Copyright (c) 2018, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>  // fprintf
#include <string.h> //strlen
#include <unistd.h> //usleep

#include "sensirion_uart.h"
#include "sps30.h"

/**
 * TO USE CONSOLE OUTPUT (PRINTF) AND WAIT (SLEEP) PLEASE ADAPT THEM TO YOUR
 * PLATFORM
 */
//#define fprintf(...)

uint8_t AUTO_CLEAN_DAYS = 1;
char serial[SPS30_MAX_SERIAL_LEN];
struct sps30_measurement m;
size_t MAX_INIT = 20;

//config file stuff
#define MAX_CFG_KEY_LEN 32
#define MAX_CFG_VAL_LEN 1024

typedef struct {
  uint8_t print_sn;
  uint8_t use_zmq;
  char server_bind[MAX_CFG_VAL_LEN];
}server_config;

// Error codes
#define OK 0
#define ERR_PROBE_FAILED -1

int main(void) {

  int16_t ret;

  while (sensirion_uart_open() != 0) {
    fprintf(stderr,"UART init failed\n");
    sensirion_sleep_usec(1000000); /* sleep for 1s */
  }

  /* Busy loop for initialization, because the main loop does not work without
   * a sensor.
   */
  size_t initialize_attempts=0;

  while (sps30_probe() != 0 && initialize_attempts < MAX_INIT) {
    initialize_attempts ++ ;
    fprintf(stderr,"SPS30 sensor probing failed %d of %d attempts\n",initialize_attempts, MAX_INIT);
    sensirion_sleep_usec(1000000); /* sleep for 1s */
  }

  if (initialize_attempts >= MAX_INIT){
    fprintf(stderr,"SPS30 sensor probing failed!\n");
    return ERR_PROBE_FAILED;
  }

  fprintf(stderr,"SPS30 sensor probing successful\n");

  struct sps30_version_information version_information;
  ret = sps30_read_version(&version_information);
  if (ret) {
    fprintf(stderr,"error %d reading version information\n", ret);
  } else {
    fprintf(stderr,"FW: %u.%u HW: %u, SHDLC: %u.%u\n",
        version_information.firmware_major,
        version_information.firmware_minor,
        version_information.hardware_revision,
        version_information.shdlc_major,
        version_information.shdlc_minor);
  }

  ret = sps30_get_serial(serial);
  if (ret){
    fprintf(stderr,"error %d reading serial\n", ret);
    snprintf(serial,SPS30_MAX_SERIAL_LEN,"Unknown s/n");
  }
  else{
    fprintf(stderr,"SPS30 Serial: %s\n", serial);
  }

  size_t sn_len = strlen(serial);
  if (sn_len > SPS30_MAX_SERIAL_LEN){
    sn_len = SPS30_MAX_SERIAL_LEN;
  }

  ret = sps30_set_fan_auto_cleaning_interval_days(AUTO_CLEAN_DAYS);
  if (ret)
    fprintf(stderr,"error %d setting the auto-clean interval\n", ret);

  while (1) {
    ret = sps30_start_measurement();
    if (ret < 0) {
      fprintf(stderr,"error starting measurement\n");
    }

    fprintf(stderr,"measurements started\n");

    for (int i = 0; i < 60; ++i) {

      ret = sps30_read_measurement(&m);
      if (ret < 0) {
        fprintf(stderr,"error reading measurement\n");
      } else {
        if (SPS30_IS_ERR_STATE(ret)) {
          fprintf(  stderr,  
              "Chip state: %u - measurements may not be accurate\n",
              SPS30_GET_ERR_STATE(ret));
        }

        fprintf(stdout,"%.*s"
            " pm1.0:%0.2f"
            " pm2.5:%0.2f"
            " pm4.0:%0.2f"
            " pm10.0:%0.2f"
            " nc0.5:%0.2f"
            " nc1.0:%0.2f"
            " nc2.5:%0.2f"
            " nc4.5:%0.2f"
            " nc10.0:%0.2f"
            " typical-size:%0.2f\n",
            sn_len,
            serial,
            m.mc_1p0, m.mc_2p5, m.mc_4p0, m.mc_10p0, m.nc_0p5,
            m.nc_1p0, m.nc_2p5, m.nc_4p0, m.nc_10p0,
            m.typical_particle_size);
      }
      
      sensirion_sleep_usec(1000000); /* sleep for 1s */
    }

    /* 
     * Stop measurement for 1min to preserve power. Also enter sleep mode
     * if the firmware version is >=2.0.
     */
    ret = sps30_stop_measurement();
    if (ret) {
      fprintf(stderr,"Stopping measurement failed\n");
    }

    if (version_information.firmware_major >= 2) {
      ret = sps30_sleep();
      if (ret) {
        fprintf(stderr,"Entering sleep failed\n");
      }
    }

    fprintf(stderr,"No measurements for 1 minute\n");
    sensirion_sleep_usec(1000000 * 60);

    if (version_information.firmware_major >= 2) {
      ret = sps30_wake_up();
      if (ret) {
        fprintf(stderr,"Error %i waking up sensor\n", ret);
      }
    }
  }

  if (sensirion_uart_close() != 0)
    fprintf(stderr,"failed to close UART\n");

  return OK;
}
