/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2020 - 2022 Impinj, Inc. All rights reserved.               *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 Ex10 chip sensor measurement example

    This example measures all of the Aux ADC inputs and the SJC solution
 determined by the SJC Op inside the reader chip firmware, sweeping TX
 power and frequency to characterize the behavior of a hardware with an
 attached antenna or load.
    This allows characterization of LO and RX power measurements to determine
 intrinsic self-jammer performance (using a 50 Ohm termaination at the antenna
 port) and performance with a specific antenna.
    If SJC errors occur, modifications may be made to the cw_on op to ignore
 those errors, allowing the example to gather data in the invalid
 configurations.
    The results are printed in a CSV format so they can be easily analyzed in
 software like Excel.
 *****************************************************************************/

#include <assert.h>
#include <stdio.h>

#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"

#include "board/time_helpers.h"

/* Settings used when running this example */
static const uint8_t antenna           = 1;
uint16_t             cw_time_on_ms     = 50;
uint16_t             ADC_START         = 0x00;
uint16_t             ADC_CHANNEL_COUNT = 15;

static int measure_chip_sensors(struct Ex10Interfaces ex10_iface)
{
    // print CSV headers
    printf("Starting test characterization of PDETs and SJCs\n");
    printf(
        "Carrier frequency (kHz),TX Power Target (cdBm),"
        "LO PDET0,LO PDET1,LO PDET2,LO PDET3,");
    printf("RX PDET0,RX PDET1,RX PDET2,RX PDET3,");
    printf("TestMux0,TestMux1,TestMux2,TestMux3,");
    printf("Temp ADC,LO PDET SUM,RX PDET SUM,");
    printf(
        "SJC atten,CDAC I value,CDAC I residue,"
        "CDAC Q value,CDAC Q residue,");
    printf("\n");

    // define sweep parameters
    uint32_t frequency_kHz_init     = 908750u;   //start at 902750
    uint32_t frequency_kHz_maximum  = 912250;    // stop at 927250
    uint32_t frequency_kHz_stepsize = 500;
    uint16_t power_tx_cdBm_init     = 0;
    uint16_t power_tx_cdBm_maximum  = 3210;
    uint16_t power_tx_cdB_stepsize  = 300;
    //uint32_t frequency_kHz = frequency_kHz_init; 


    // sweep frequency and power and measure sensors
    for (uint32_t frequency_kHz = frequency_kHz_init;
         frequency_kHz <= frequency_kHz_maximum;
         frequency_kHz += frequency_kHz_stepsize)
    {
        // sweep frequency and power and measure sensors
        for (uint32_t i = 0; i <= 2; i+= 1)     // from 0 to i, so 0 to 9 = 10 times
    
        {   
            for (uint16_t power_tx_cdBm = power_tx_cdBm_init;
             power_tx_cdBm <= power_tx_cdBm_maximum;
             power_tx_cdBm += power_tx_cdB_stepsize)
            {
            bool     transmitting = false;
            uint32_t start_time   = get_ex10_time_helpers()->time_now();
            while (get_ex10_time_helpers()->time_elapsed(start_time) <
                   cw_time_on_ms)
            {
                if (!transmitting)
                {
                    uint32_t const remain_on = false;  // Allow regulatory times
                                                       // to turn off the
                                                       // carrier
                    // turn on carrier
                    struct OpCompletionStatus op_status =
                        ex10_iface.reader->cw_test(antenna,
                                                   5,
                                                   power_tx_cdBm,
                                                   frequency_kHz,
                                                   remain_on);
                    if (op_status.error_occurred)
                    {
                        // SJC errors could occur here, ignore them so we get
                        // the solution anyway
                    }
                    transmitting = true;
                }

                struct EventFifoPacket const* packet =
                    ex10_iface.reader->packet_peek();
                if (packet)
                {
                    if (packet->packet_type == TxRampDown)
                    {
                        transmitting = false;
                    }
                    ex10_iface.reader->packet_remove();
                }
            }

            // print out carrier frequency and TX power target
            printf("%6d,", frequency_kHz);
            printf("%4d,", power_tx_cdBm);

            // measure Aux ADC channels
            uint16_t adc_result[16];
            ex10_iface.ops->measure_aux_adc(
                ADC_START, ADC_CHANNEL_COUNT, adc_result);

            // print out Aux ADC channel measurements
            for (uint16_t adc_result_index = 0;
                 adc_result_index < ADC_CHANNEL_COUNT;
                 adc_result_index++)
            {
                printf("%4d,", adc_result[adc_result_index]);
            }

            // get SJC solution (SJC was already performed as part of cw_on)
            const struct Ex10SjcAccessor* sjc_accessor =
                ex10_iface.ops->get_sjc();
            struct SjcResultPair sjc_results = sjc_accessor->get_sjc_results();
            const struct RxGainControlFields* rx_gain_control =
                ex10_iface.ops->get_current_analog_rx_config();

            // print out SJC solution
            printf("%3d,", rx_gain_control->rx_atten);
            printf("%3d,", sjc_results.i.cdac);
            printf("%3d,", sjc_results.i.residue);
            printf("%3d,", sjc_results.q.cdac);
            printf("%3d,", sjc_results.q.residue);

            printf("\n");

            // ramp down and move to next configuration for test
            ex10_iface.reader->stop_transmitting();
            }
        } 
    }


    return 0;
}

int main(void)
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");

    int result = measure_chip_sensors(ex10_iface);

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
