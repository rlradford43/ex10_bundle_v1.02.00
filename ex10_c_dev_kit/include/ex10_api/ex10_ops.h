/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2020 - 2021 Impinj, Inc. All rights reserved.               *
 *                                                                           *
 *****************************************************************************/

#pragma once

#include <sys/types.h>

#include "board/ex10_gpio.h"
#include "ex10_api/ex10_protocol.h"
#include "ex10_api/fifo_buffer_list.h"
#include "ex10_api/gen2_commands.h"
#include "ex10_api/regions_table.h"
#include "ex10_api/sjc_accessor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct GpioControlFields
 * Used to control and read the GPIO outputs. Each Ex10 DIGITAL_IO pin is
 * assigned a numeric value represented as (1u << DIGITAL_IO[N]);
 */
struct GpioControlFields
{
    /// Set and get the GPIO pins output enable states.
    uint32_t output_enable;
    /// Set and get the GPIO pins output level states.
    uint32_t output_level;
};

struct PowerConfigs
{
    uint8_t                             tx_atten;
    int16_t                             tx_scalar;
    uint32_t                            dc_offset;
    uint32_t                            adc_target;
    uint16_t                            loop_stop_threshold;
    uint16_t                            op_error_threshold;
    uint16_t                            loop_gain_divisor;
    uint32_t                            max_iterations;
    enum AuxAdcControlChannelEnableBits power_detector_adc;
};

enum Gen2SequencerResult
{
    Gen2SequencerSuccess = 0,
    Gen2SequencerError,
    Gen2SequencerIndexNotFound,
    Gen2SequencerCommandMismatch,
    Gen2SequencerLengthMismatch,
    _SEQUENCER_RESULT_MAX
};

static uint32_t const ATestMuxDisable = 0u;

/**
 * Allows for the routing of:
 * - AUX DAC[0] to ANA_TEST3
 * - AUX DAC[1] to ANA_TEST2
 */
static uint32_t const ATestMuxAuxDac = (1u << 19u);

/**
 * Allows for the routing of:
 * - AUX ADC [8] to ANA_TEST0
 * - AUX ADC [9] to ANA_TEST1
 * - AUX ADC[10] to ANA_TEST2
 * - AUX ADC[11] to ANA_TEST3
 */
static uint32_t const ATestMuxAuxAdc = (1u << 20u);

struct Ex10Ops
{
    /**
     * Initialize the Ex10 Ops instance
     * @param ex10_protocol An Ex10Protocol instance
     */
    void (*init)(struct Ex10Protocol const* ex10_protocol);

    /// Initialize the Impinj Reader Chip at the Ex10Ops layer.
    struct OpCompletionStatus (*init_ex10)(void);

    /// Cleans up dependencies between the SDK and Impinj Reader Chip.
    void (*release)(void);

    /**
     * Register an optional callback for EventFifo data events.
     * @param fifo_cb This callback is triggered when the interrupt_cb
     * registered through register_interrupt_callback returns true.
     * @see register_interrupt_callback
     */
    void (*register_fifo_data_callback)(
        void (*fifo_cb)(struct FifoBufferNode*));

    /**
     * Enable specified interrupts and register a callback
     * @param enable_mask The mask of interrupts to enable.
     * @param interrupt_cb A function called when an enabled interrupt fires.
     * The callback receives the interrupt status bits via argument and it
     * should return true or false. If the return value is true then the
     * fifo_data callback is triggered. @see register_fifo_data_callback.
     */
    void (*register_interrupt_callback)(
        struct InterruptMaskFields enable_mask,
        bool (*interrupt_cb)(struct InterruptStatusFields));

    /// Unregister the callback used to deal with fifo data
    void (*unregister_fifo_data_callback)(void);

    /**
     * Unregister the callback for interrupts and write to the Ex10 device to
     * disable the interrupts.
     */
    void (*unregister_interrupt_callback)(void);

    /// Allows access to the underlying Ex10Protocol instance.
    struct Ex10Protocol const* (*ex10_protocol)(void);

    /**
     * Return whether or not CW is currently on.
     * @param cw_is_on A bool of whether CW is currently on.
     */
    bool (*get_cw_is_on)(void);

    /**
     * Read the OpsStatus register.
     * @return struct OpsStatusFields The fields of the 'OpsStatus' register.
     */
    struct OpsStatusFields (*read_ops_status)(void);

    /**
     * Start a log test op.
     * This op periodically writes a log entry to the debug port.
     * Each log entry consists of a timestamp, followed by a requested
     * number of repetitions of that timestamp and an end-of-line.
     * This op will run until stopped.
     *
     * @param period period (milliseconds) of log entries
     * @param word_repeat Number of timestamp repetitions in each log entry
     */
    void (*start_log_test)(uint32_t period, uint32_t word_repeat);

    /**
     * Set the ATEST multiplexer channel routings by running the SetATestMuxOp.
     * See the documentation for a complete description.
     * @see ATestMuxAuxDac, @see ATestMuxAuxAdc
     */
    void (*set_atest_mux)(uint32_t atest_mux_0,
                          uint32_t atest_mux_1,
                          uint32_t atest_mux_2,
                          uint32_t atest_mux_3);

    /**
     * This executes the MeasureAdcOp operation which performs AUX ADC for each
     * multiplexer input specified. Once the conversions are completed the
     * ADC results are placed in the adc_results output parameter.
     * @note This runs the MeasureAdcOp to completion.
     *
     * @param adc_channel_start Determines the starting channel on which to
     *          begin ADC conversions.
     * @param num_channels The number of ADC conversions to perform starting
     *          with the adc_channel_start. Each conversion will be performed
     *          on the next multiplexer input. The number of channels converted
     *          will be limited to be within the valid AuxAdcResults range.
     * @param adc_results [out] The array into which of ADC conversion results
     *          will be contained. The adc_results must be able to fit
     *          num_channels conversion results.
     * @return Info about any encountered errors.
     */
    struct OpCompletionStatus (*measure_aux_adc)(
        enum AuxAdcResultsAdcResult adc_channel_start,
        uint16_t                    num_channels,
        uint16_t*                   adc_results);

    /**
     * This executes the SetDacOp operation which writes values to the AUX DAC
     * for each channel specified. When the SetDacOp has completed the digital
     * to analog conversions will have completed.
     *
     * @note There are 2 AUX DAC channels which can be written.
     *       - If a channel is ommitted from the DAC list then that channel
     *         is disabled and its output will be zero.
     *       - If more channels are set outside the range of the DAC list,
     *         then the out of bounds values are ignored.
     *
     * @param dac_channel_start Determines the starting DAC channel.
     * @param num_channels The number of DAC channel conversions to perform.
     * @param dac_values   An array of DAC channel data containing unsigned
     *                     10-bit values for conversion. The size of this array
     *                     must be uint16_t[num_channels].
     */
    void (*set_aux_dac)(uint16_t        dac_channel_start,
                        uint16_t        num_channels,
                        uint16_t const* dac_values);

    /**
     * This function executes the SetRfModeOp which will set the Ex10 modem
     * registers for use with a specific RF mode.
     *
     * @param rf_mode The requested RF mode.
     */
    void (*set_rf_mode)(uint16_t rf_mode);

    /**
     * Determine whether the RF mode is intended for use as a Dense Reader Mode
     * (DRM) or not.
     *
     * @param rf_mode The requested RF mode.
     *
     * @return bool true if the RF mode is a Dense Reader Mode, false if not.
     */
    bool (*rf_mode_is_drm)(uint16_t rf_mode);

    /**
     * Enable the transmitter and begin CW transmission. Sets a timer for any
     * non-zero regulatory dwell time.
     * - dc_offset: DC offset of the transmitter in the EX10.
     * - nominal_stop_time: Ex10 will complete the ongoing Gen2 operation until
     *   it reaches a Query, QueryAdj or QueryRep and ramp down Tx.
     * - extended_stop_time: Ex10 will terminate any ongoing Gen2 operation
     *   and immediately ramp down Tx.
     * - regulatory_stop_time: Ex10 will terminate any ongoing Gen2 operation,
     *   immediately ramp down Tx and set the TX scalar to 0.
     */
    void (*tx_ramp_up)(int32_t                        dc_offset,
                       struct RegulatoryTimers const* timer_config);

    /**
     * Disable the transmitter
     */
    void (*tx_ramp_down)(void);

    /**
     * Set the TX coarse gain settings.
     * @param tx_atten The CT filter attenuation.
     */
    void (*set_tx_coarse_gain)(uint8_t tx_atten);

    /**
     * Tx fine gain value.
     * @param tx_scalar new fine gain value
     */
    void (*set_tx_fine_gain)(int16_t tx_scalar);

    /**
     * Power up/down the LDOs and bias currents for the Ex10 radio
     * by running the RadioPowerControlOp.
     *
     * @param enable If true,  turns on  the Ex10 analog power supplies.
     *               If false, turns off the Ex10 analog power supplies.
     */
    void (*radio_power_control)(bool enable);

    /**
     * Get the last used receiver gain.
     * @param return The stored receiver gain of whatever was last passed to
     * set_analog_rx_config.
     */
    const struct RxGainControlFields* (*get_current_analog_rx_config)(void);

    /**
     * Set individual block gain of the receiver.
     * @param analog_rx_fields The gain we set the receiver to. This value is
     * stored in a state variable for later retrieval.
     */
    void (*set_analog_rx_config)(
        struct RxGainControlFields const* analog_rx_fields);

    /**
     * Setup and run the op to get the RSSI
     * @note Once run, the RSSI measurements must be retrieved through the
     * event FIFO
     * @param rssi_count           The integration count to used for
     * measurement. The recommended value is 0x0Fu.
     */
    void (*measure_rssi)(uint8_t rssi_count);

    /**
     * Returns the default Rx analog settings used in the listen before talk op.
     */
    struct RxGainControlFields (*get_default_lbt_rx_analog_configs)(void);

    /**
     * Runs the listen before talk op.
     * The op goes through the following steps:
     * 1. Set the synthesizer to the offset frequency
     * 2. Sets the internal low pass filter
     * 3. Runs the RSSI measurement.
     * For these steps to work, the host passes in configurations for the
     * synthesizer lock and LBT offset frequency. When the op measures the RSSI,
     * the measurements are sent out through an RSSI measurement FIFO event.
     * @param r_divider_index Index (0-7) of a divider value for RfSynthesizer,
     * divides down from FREF.
     * @param n_divider            N divider for the RfSynthesizer
     * @param offset_frequency_khz The frequency offset for lbt.
     * @param rssi_count           The integration count to used for
     * measurement. The recommended value is 0x0Fu.
     * @note: Before running LBT, the host must have run 2 ops: set_gpio,
     * set_analog_rx_config.
     * @note: CW must also be ramped down.
     * @note: The op leaves the synthesizer locked to the offset frequency.
     */
    void (*run_listen_before_talk)(uint8_t  r_divider_index,
                                   uint16_t n_divider,
                                   int32_t  offset_frequency_khz,
                                   uint8_t  rssi_count);

    /**
     * Run the us timer start op. This starts a timer which one can check
     * completion of via the UsTimerWaitOp.
     * @note If the op is run while a timer is already started, the current
     * timer will be cancelled and a new one will be started.
     * @param delay_us The time in microseconds for the timer to run.
     */
    void (*start_timer_op)(uint32_t delay_us);

    /**
     * Run the us timer wait op. This waits for the timer to complete which was
     * started by the UsTimerStartOp.
     * @note If the timer is done or no timer was started, the op will return
     * immediately.
     */
    void (*wait_timer_op)(void);

    /**
     * Lock the RF synthesizer to the target frequency
     * @param r_divider_index Index (0-7) of a divider value for
     *                       RfSynthesizer, divides down from FREF.
     * @param n_divider       N divider for the RfSynthesizer
     */
    void (*lock_synthesizer)(uint8_t r_divider_index, uint16_t n_divider);

    /**
     * Start event FIFO test op.
     * This op periodically pushes debug packets into the event FIFO.
     * @param period period in milliseconds of event FIFO debug packets
     * @param num_words payload size in each event FIFO debug packet
     */
    void (*start_event_fifo_test)(uint32_t period, uint8_t num_words);

    /**
     * Inject a custom packet into the Event Fifo Stream.
     * This is not an op, but a protocol feature that will inject an EventFifo
     * packet into the Ex10 Event Fifo stream.
     */
    void (*insert_fifo_event)(const bool                    trigger_irq,
                              const struct EventFifoPacket* event_packet);

    /**
     * Enable the SDD log outputs and log speed
     * @param enables the structure with the individual enables
     * @param speed_mhz  The speed of the sdd SPI clock (1-24 MHz)
     */
    void (*enable_sdd_logs)(const struct LogEnablesFields enables,
                            const uint8_t                 speed_mhz);

    /**
     * Send the enabled Gen2 Access command sequence for  halted which
     * are already stored in the Gen2TxBuffer.
     *
     * @note The modem MUST be in the halted state.
     */
    void (*send_gen2_halted_sequence)(void);

    /**
     * Release the currently halted on tag and move on to the next.
     *
     * @param nak Indicates whether to nak the tag or not.
     *
     * @note The modem MUST be in the halted state.
     */
    void (*continue_from_halted)(bool nak);

    /**
     * Run the RunRxSjcOp.
     * The settings by which the SJC will be configured are set up in
     * the ops init.
     */
    struct OpCompletionStatus (*run_sjc)(void);

    /** Get the Ex10SjcAccessor interface. */
    struct Ex10SjcAccessor const* (*get_sjc)(void);

    /**
     * Wait for prevously started op to complete.
     *
     * @return Info about the completed op.
     */
    struct OpCompletionStatus (*wait_op_completion)(void);

    /**
     * Starts the aggregate op.
     */
    void (*run_aggregate_op)(void);

    /**
     * Wait for prevously started op to complete.
     *
     * @param timeout_ms Function returns false if the op takes more
     *                   time than this number of milliseconds.
     * @return Info about the completed op.
     */
    struct OpCompletionStatus (*wait_op_completion_with_timeout)(
        uint32_t timeout_ms);

    /// Stop a previously started op.
    void (*stop_op)(void);

    /// Get the current GPIO output levels and enable.
    struct GpioControlFields (*get_gpio)(void);

    /**
     * Set the GPIO levels and enables using the SetGpioOp.
     * There are 22 Ex10 GPIOs.
     * Each GPIO can be set as an output by setting the appropriate bit
     * in the gpio level and enable fields. This function uses the
     * SetGpioOp to set the GPIO levels and enables.
     *
     * @param gpio_levels  A bit-field which sets each GPIO pin output level.
     * @param gpio_enables A bit-field which enables each GPIO pin as an output.
     */
    void (*set_gpio)(uint32_t gpio_levels, uint32_t gpio_enables);

    /**
     * Set the GPIO levels and enables using the SetClearGpioPinsOp.
     * There are 22 Ex10 GPIOs. Each GPIO can be adressed within each bit-field
     * by setting or clearing the appropriate bit (1u << pin_number).
     *
     * Set the GPIO levels and enable. There are 22 Ex10 GPIOs.
     * The GPIOs are labelled DIGITAL_IO[N] on the Imping Reader Chip schematic.
     *
     * @param gpio_pins_set_clear The GPIO pin bit-fields to set and clear.
     * @see struct GpioPinsSetClear
     */
    void (*set_clear_gpio_pins)(
        struct GpioPinsSetClear const* gpio_pins_set_clear);

    /**
     * Start a single inventory round
     * @param configs inventory round configuration
     */
    void (*start_inventory_round)(
        struct InventoryRoundControlFields const*   configs,
        struct InventoryRoundControl_2Fields const* configs_2);

    /**
     * Enable PRBS modulation
     */
    void (*start_prbs)(void);

    /**
     * Set the HPF override settings and run the HPF override test op.
     */
    void (*start_hpf_override_test_op)(
        struct HpfOverrideSettingsFields const* hpf_settings);

    /**
     * Start the ETSI burst test
     * @param on_time_ms       The time for which cw remains on after ramp up
     * @param off_time_ms      Controls the time cw remains off after ramp down
     * See inventory() for other parameter descriptions.
     * @return Info about any encountered errors.
     */
    struct OpCompletionStatus (*start_etsi_burst)(
        struct InventoryRoundControlFields const*   inventory_config,
        struct InventoryRoundControl_2Fields const* inventory_config_2,
        struct GpioPinsSetClear const*              gpio_controls,
        uint16_t                                    rf_mode,
        struct PowerConfigs*                        power_config,
        struct RfSynthesizerControlFields const*    synth_control,
        struct RegulatoryTimers const*              timer_config,
        uint16_t                                    on_time_ms,
        uint16_t                                    off_time_ms);

    /**
     * Enable the Ber Test
     * @param num_bits       The number of bits to send
     * @param num_packets    The number of packets to transmit
     * @param delimiter_only Determine whether to use a delimiter only instead
     * of a full query
     */
    void (*start_ber_test)(uint16_t num_bits,
                           uint16_t num_packets,
                           bool     delimiter_only);

    /**
     * Run the send select op. The op will look through the
     * Gen2SelectEnable register and begin send all enabled commands
     * from the gen2 tx buffer.
     */
    void (*send_select)(void);

    /**
     * Allows writing the calibration data of the EX10 device.
     *
     * @param data_ptr A pointer to the calibration data
     * @param write_length The number of bytes to write
     *
     * A CRC-16-CCITT will be calculated across the data and appended to the
     * written data.
     */
    void (*write_calibration_page)(uint8_t const* data_ptr,
                                   size_t         write_length);

    /**
     * Ramp down and turn off the radio transmitter.
     * @note The radio remains powered on but the transmitter is stopped.
     */
    void (*cw_off)(void);

    /**
     * Ramp power while transmitting CW and prepare the receiver
     * @see inventory for parameter descriptions.
     * @return Info about any encountered errors.
     */
    struct OpCompletionStatus (*cw_on)(
        struct GpioPinsSetClear const*             gpio_pins_set_clear,
        uint16_t                                   rf_mode,
        struct PowerConfigs*                       power_config,
        struct RfSynthesizerControlFields const*   synth_control,
        struct RegulatoryTimers const*             timer_config,
        struct PowerDroopCompensationFields const* droop_comp);

    /**
     * Ramps to the target transmit power.
     * @see inventory for parameter descriptions.
     * @return Info about any encountered errors.
     */
    struct OpCompletionStatus (*ramp_transmit_power)(
        struct PowerConfigs*           power_config,
        struct RegulatoryTimers const* timer_config);

    /**
     * Ramps up power, runs SJC, and starts an inventory round.
     * @param gpio_pins_set_clear   @see struct GpioPinsSetClear
     * @param rf_mode               An integer indicating the RF Mode.
     * @param power_config          @see struct PowerConfigs
     * @param synth_config          @see struct RfSynthesizerControlFields
     * @param timer_config          @see struct RegulatoryTimers
     * @param inventory_config      @see struct InventoryRoundControlFields
     * @param inventory_config_2    @see struct InventoryRoundControl_2Fields
     * @param run_select_op         Whether or not to run the send select op
                                    and send any enabled select commands in the
                                    gen2 command buffer.
     * @param droop_comp            @see struct PowerDroopCompensationFields
     * @return Info about any encountered errors.
     */
    struct OpCompletionStatus (*inventory)(
        struct GpioPinsSetClear const*              gpio_pins_set_clear,
        uint16_t                                    rf_mode,
        struct PowerConfigs*                        power_config,
        struct RfSynthesizerControlFields const*    synth_control,
        struct RegulatoryTimers const*              timer_config,
        struct InventoryRoundControlFields const*   inventory_config,
        struct InventoryRoundControl_2Fields const* inventory_config_2,
        bool                                        run_select_op,
        struct PowerDroopCompensationFields const*  droop_comp);

    /**
     * Return the current 32 bit time (microseconds) since start from the
     * device.
     */
    uint32_t (*get_device_time)(void);

    /**
     * Enable power droop compensation.
     *
     * @param compensation          @see struct PowerDroopCompensationFields
     *
     * Tx RF power can droop as the transmitter warms up, but enabling
     * compensation will periodically adjust TxFineGain so that power is
     * closer to the target.  This feature is enabled by default.
     */
    void (*enable_droop_compensation)(
        struct PowerDroopCompensationFields const* compensation);

    /**
     * Disable power droop compensation.
     */
    void (*disable_droop_compensation)(void);

    /**
     * Registers the function pointer to call before ramping when callings the
     * ops layer cw_on function. The ops cw_on ramp is used from all the reader
     * layer functions as the one pathway to ramping up. This callback will
     * therefore be used by inventory, continuous_inventory, cw_test, ber, prbs,
     * etc.
     * @param pre_cb The function pointer to register. The function takes no
     * parameters and returns a bool. This bool shall return true on success,
     * and false otherwise. In the case of an error, and error handling should
     * be done by the callback context since it knows what is appropriate to do
     * for the error. The returned bool will just cause the cw_on call to return
     * with an error occurred.
     */
    void (*register_pre_rampup_callback)(bool (*pre_cb)(void));

    /**
     * Registers the function pointer to call after ramping up when callings the
     * ops layer cw_on function. The ops cw_on ramp is used from all the reader
     * layer functions as the one pathway to ramping up. This callback will
     * therefore be used by inventory, continuous_inventory, cw_test, ber, prbs,
     * etc.
     * @param post_cb The function pointer to register. The function takes no
     * parameters and returns a bool. This bool shall return true on success,
     * and false otherwise. In the case of an error, and error handling should
     * be done by the callback context since it knows what is appropriate to do
     * for the error. The returned bool will just cause the cw_on call to return
     * with an error occurred.
     */
    void (*register_post_rampup_callback)(bool (*post_cb)(void));

    /**
     * Sets the function pointer called pre rampup to NULL. For more
     * information, see register_pre_ramp_callback.
     */
    void (*unregister_pre_rampup_callback)(void);

    /**
     * Sets the function pointer called post rampup to NULL. For more
     * information, see register_post_ramp_callback.
     */
    void (*unregister_post_rampup_callback)(void);

    /// A const pointer to the struct OpCompletionStatus,
    /// initialized with a setting consistent with the No Error condition.
    struct OpCompletionStatus const* op_error_none;
};

struct Ex10Ops const* get_ex10_ops(void);

#ifdef __cplusplus
}
#endif
