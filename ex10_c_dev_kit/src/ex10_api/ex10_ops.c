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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "board/board_spec.h"
#include "board/region.h"
#include "ex10_api/aggregate_op_builder.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_ops.h"
#include "ex10_api/fifo_buffer_list.h"
#include "ex10_api/sjc_accessor.h"
#include "ex10_api/trace.h"


/// An error free OpCompletionStatus struct.
static struct OpCompletionStatus const op_error_none = {
    .error_occurred = false,
    .ops_status =
        {
            .op_id     = Idle,
            .busy      = false,
            .Reserved0 = 0u,
            .error     = ErrorNone,
            .rfu       = 0u,
        },
    .command_error             = Success,
    .timeout_error             = NoTimeout,
    .aggregate_buffer_overflow = false,
};

/**
 * @struct OpVariables
 * Local variables to be used with the static functions outline by the
 * of the Ex10Ops struct.
 */
struct OpVariables
{
    bool                              gen2_tx_buffer_dirty;
    struct Ex10Protocol const*        ex10_protocol;
    struct Ex10SjcAccessor const*     sjc_instance;
    struct Ex10Gen2Commands const*    gen2_commands;
    struct Ex10Region const*          region;
    struct RxGainControlFields        stored_analog_rx_fields;
    struct RfSynthesizerControlFields stored_synth_control;
    bool (*pre_rampup_callback)(void);
    bool (*post_rampup_callback)(void);
};

static struct OpVariables op_variables = {
    .gen2_tx_buffer_dirty    = true,
    .ex10_protocol           = NULL,
    .sjc_instance            = NULL,
    .gen2_commands           = NULL,
    .region                  = NULL,
    .stored_analog_rx_fields = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .stored_synth_control    = {0, 0, 0, 0, 0},
    .pre_rampup_callback     = NULL,
    .post_rampup_callback    = NULL,
};

static void set_analog_rx_config(struct RxGainControlFields const*);
static struct OpCompletionStatus wait_op_completion(void);
static struct OpCompletionStatus wait_op_completion_with_timeout(uint32_t);
static void                      enable_droop_compensation(
                         struct PowerDroopCompensationFields const* compensation);
static void disable_droop_compensation(void);


static void unregister_fifo_data_callback(void)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->unregister_fifo_data_callback();
}

static void unregister_interrupt_callback(void)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->unregister_interrupt_callback();
}

static void unregister_pre_rampup_callback(void)
{
    op_variables.pre_rampup_callback = NULL;
}

static void unregister_post_rampup_callback(void)
{
    op_variables.post_rampup_callback = NULL;
}

static void init(struct Ex10Protocol const* ex10_protocol)
{
    op_variables.ex10_protocol = ex10_protocol;
    op_variables.sjc_instance  = get_ex10_sjc();
    op_variables.gen2_commands = get_ex10_gen2_commands();
    op_variables.region        = get_ex10_region();

    unregister_fifo_data_callback();
}

static struct OpCompletionStatus init_ex10(void)
{
    unregister_interrupt_callback();

    op_variables.sjc_instance->init(op_variables.ex10_protocol);

    // Used to determine if gen2 command sequencer should write entire buffer
    op_variables.gen2_tx_buffer_dirty = true;

    // A cached copy of the RfSynthesizerControlFields settings.
    memset(&op_variables.stored_synth_control,
           0u,
           sizeof(op_variables.stored_synth_control));

    // A cached copy of the RxGainControl settings.
    op_variables.stored_analog_rx_fields =
        *get_ex10_board_spec()->get_default_rx_analog_config();
    set_analog_rx_config(&op_variables.stored_analog_rx_fields);
    struct OpCompletionStatus const op_error = wait_op_completion();

    // Setting the RxGains will have triggered on Op completion interrupt.
    // Clear it to start fresh and new.
    struct InterruptStatusFields irq_status;
    op_variables.ex10_protocol->read(&interrupt_status_reg, &irq_status);

    return op_error;
}

static void release(void)
{
    if (op_variables.ex10_protocol != NULL)
    {
        unregister_fifo_data_callback();
        unregister_interrupt_callback();
        op_variables.ex10_protocol = NULL;
    }
}

static struct OpCompletionStatus wait_op_completion(void)
{
    assert(op_variables.ex10_protocol);
    return op_variables.ex10_protocol->wait_op_completion();
}

static struct OpCompletionStatus wait_op_completion_with_timeout(
    uint32_t timeout_ms)
{
    assert(op_variables.ex10_protocol);
    return op_variables.ex10_protocol->wait_op_completion_with_timeout(
        timeout_ms);
}

static void register_fifo_data_callback(void (*fifo_cb)(struct FifoBufferNode*))
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->register_fifo_data_callback(fifo_cb);
}

static void register_interrupt_callback(
    struct InterruptMaskFields enable_mask,
    bool (*interrupt_cb)(struct InterruptStatusFields))
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->register_interrupt_callback(enable_mask,
                                                            interrupt_cb);
}

static void register_pre_rampup_callback(bool (*pre_cb)(void))
{
    assert(pre_cb);
    op_variables.pre_rampup_callback = pre_cb;
}

static void register_post_rampup_callback(bool (*post_cb)(void))
{
    assert(post_cb);
    op_variables.post_rampup_callback = post_cb;
}

static struct Ex10Protocol const* ex10_protocol(void)
{
    return op_variables.ex10_protocol;
}

static bool get_cw_is_on(void)
{
    assert(op_variables.ex10_protocol);
    struct CwIsOnFields cw_is_on;
    op_variables.ex10_protocol->read(&cw_is_on_reg, &cw_is_on);
    return cw_is_on.is_on;
}

static struct OpsStatusFields read_ops_status(void)
{
    assert(op_variables.ex10_protocol);
    struct OpsStatusFields ops_status;
    op_variables.ex10_protocol->read(&ops_status_reg, &ops_status);
    return ops_status;
}

static void start_log_test(uint32_t period, uint32_t word_repeat)
{
    struct LogTestPeriodFields const     log_period      = {.period = period};
    struct LogTestWordRepeatFields const log_word_repeat = {.repeat =
                                                                word_repeat};
    struct OpsControlFields const ops_control_data       = {.op_id = LogTestOp};

    struct RegisterInfo const* const regs[] = {
        &log_test_period_reg,
        &log_test_word_repeat_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &log_period,
        &log_word_repeat,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void set_atest_mux(uint32_t atest_mux_0,
                          uint32_t atest_mux_1,
                          uint32_t atest_mux_2,
                          uint32_t atest_mux_3)
{
    assert(op_variables.ex10_protocol);

    uint32_t const atest_mux[] = {
        atest_mux_0, atest_mux_1, atest_mux_2, atest_mux_3};
    op_variables.ex10_protocol->write(&a_test_mux_reg, atest_mux);

    struct OpsControlFields const ops_control_data = {.op_id = SetATestMuxOp};
    op_variables.ex10_protocol->write(&ops_control_reg, &ops_control_data);
}

static struct OpCompletionStatus measure_aux_adc(
    enum AuxAdcResultsAdcResult adc_channel_start,
    uint16_t                    num_channels,
    uint16_t*                   adc_results)
{
    // Limit the number of ADC conversion channels to the possible range.
    assert(adc_channel_start < aux_adc_results_reg.num_entries);
    uint16_t const max_channels =
        aux_adc_results_reg.num_entries - adc_channel_start;
    num_channels = (num_channels <= max_channels) ? num_channels : max_channels;

    struct AuxAdcControlFields const adc_control = {
        .channel_enable_bits = ((1u << num_channels) - 1u) << adc_channel_start,
        .rfu                 = 0u};

    struct RegisterInfo const* const regs[] = {
        &aux_adc_control_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {.op_id = MeasureAdcOp};

    void const* buffers[] = {
        &adc_control,
        &ops_control_data,
    };

    // Run the MeasureAdcOp
    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));

    // Wait for completion so we can read from the ADC
    struct OpCompletionStatus op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    uint16_t const offset = adc_channel_start * aux_adc_results_reg.length;
    struct RegisterInfo const adc_results_reg = {
        .address     = aux_adc_results_reg.address + offset,
        .length      = aux_adc_results_reg.length,
        .num_entries = num_channels,
        .access      = ReadOnly,
    };

    op_variables.ex10_protocol->read(&adc_results_reg, adc_results);
    return op_error;
}

static void set_aux_dac(uint16_t        dac_channel_start,
                        uint16_t        num_channels,
                        uint16_t const* dac_values)
{
    // Limit the number of ADC conversion channels to the possible range.
    assert(dac_channel_start < aux_dac_settings_reg.num_entries);
    uint16_t const max_channels =
        aux_dac_settings_reg.num_entries - dac_channel_start;
    num_channels = (num_channels <= max_channels) ? num_channels : max_channels;

    struct AuxDacControlFields const dac_control = {
        .channel_enable_bits = ((1u << num_channels) - 1u) << dac_channel_start,
        .rfu                 = 0u};

    // Create new reg info based off the subset of entries to use
    uint16_t const offset = dac_channel_start * aux_dac_settings_reg.length;
    struct RegisterInfo const dac_settings_reg = {
        .address     = aux_dac_settings_reg.address + offset,
        .length      = aux_dac_settings_reg.length,
        .num_entries = num_channels,
        .access      = ReadOnly,
    };

    struct OpsControlFields const ops_control_data = {.op_id = SetDacOp};

    struct RegisterInfo const* const regs[] = {
        &aux_dac_control_reg,
        &dac_settings_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &dac_control,
        dac_values,
        &ops_control_data,
    };

    // Run the SetDacOp
    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void set_rf_mode(uint16_t mode)
{
    struct RfModeFields const        rf_mode = {.id = mode};
    struct RegisterInfo const* const regs[]  = {&rf_mode_reg, &ops_control_reg};
    struct OpsControlFields const    ops_control_data = {.op_id = SetRfModeOp};

    void const* buffers[] = {
        &rf_mode,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

// JIRA PI-26845 [C SDK] Autogenerate DRM mode table in C-SDK
static bool rf_mode_is_drm(uint16_t rf_mode)
{
    // Define baseband filter to use depending on rf_mode
    // 241 and 244 are duplicates of modes 5 and 7
    uint16_t const drm_modes[] = {
        5, 6, 7, 8, 141, 146, 186, 241, 244, 286, 342, 343, 383};

    for (uint8_t i = 0; i < sizeof(drm_modes) / sizeof(drm_modes[0]); i++)
    {
        if (rf_mode == drm_modes[i])
        {
            return true;
        }
    }

    return false;
}

static void set_regulatory_timers(struct RegulatoryTimers const* timer_config)
{
    struct NominalStopTimeFields const nominal_timer = {
        .dwell_time = timer_config->nominal};
    struct ExtendedStopTimeFields const extended_timer = {
        .dwell_time = timer_config->extended};
    struct RegulatoryStopTimeFields const regulatory_timer = {
        .dwell_time = timer_config->regulatory};
    // ETSI burst ramps up and down on the same channel
    struct EtsiBurstOffTimeFields const off_timer = {
        .off_time = timer_config->off_same_channel};

    op_variables.ex10_protocol->write(&nominal_stop_time_reg, &nominal_timer);
    op_variables.ex10_protocol->write(&extended_stop_time_reg, &extended_timer);
    op_variables.ex10_protocol->write(&regulatory_stop_time_reg,
                                      &regulatory_timer);
    op_variables.ex10_protocol->write(&etsi_burst_off_time_reg, &off_timer);
}

static void tx_ramp_up(int32_t                        dc_offset,
                       struct RegulatoryTimers const* timer_config)
{
    struct DcOffsetFields const   offset           = {.offset = dc_offset};
    struct OpsControlFields const ops_control_data = {.op_id = TxRampUpOp};

    set_regulatory_timers(timer_config);

    struct RegisterInfo const* const regs[] = {
        &dc_offset_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &offset,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void tx_ramp_down(void)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->start_op(TxRampDownOp);
}

static void set_tx_coarse_gain(uint8_t tx_atten)
{
    struct TxCoarseGainFields const coarse_gain      = {.tx_atten = tx_atten};
    struct OpsControlFields const   ops_control_data = {.op_id =
                                                          SetTxCoarseGainOp};

    struct RegisterInfo const* const regs[] = {
        &tx_coarse_gain_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &coarse_gain,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void set_tx_fine_gain(int16_t tx_scalar)
{
    struct TxFineGainFields const tx_fine_gain     = {.tx_scalar = tx_scalar};
    struct OpsControlFields const ops_control_data = {.op_id = SetTxFineGainOp};

    struct RegisterInfo const* const regs[] = {
        &tx_fine_gain_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &tx_fine_gain,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void radio_power_control(bool enable)
{
    struct AnalogEnableFields const analog_enable    = {.all = enable};
    struct OpsControlFields const   ops_control_data = {
        .op_id = RadioPowerControlOp,
    };

    struct RegisterInfo const* const regs[] = {
        &analog_enable_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &analog_enable,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static const struct RxGainControlFields* get_current_analog_rx_config(void)
{
    return &op_variables.stored_analog_rx_fields;
}

static void set_analog_rx_config(
    struct RxGainControlFields const* analog_rx_fields)
{
    // Updated the locally stored analog rx settings
    assert(analog_rx_fields);
    op_variables.stored_analog_rx_fields = *analog_rx_fields;

    struct RegisterInfo const* const regs[] = {
        &rx_gain_control_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {.op_id = SetRxGainOp};

    void const* buffers[] = {
        analog_rx_fields,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void measure_rssi(uint8_t rssi_count)
{
    assert(op_variables.ex10_protocol);

    struct MeasureRssiCountFields const rssi_count_fields = {.samples =
                                                                 rssi_count};

    struct RegisterInfo const* const regs[] = {
        &measure_rssi_count_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {.op_id = MeasureRssiOp};

    void const* buffers[] = {
        &rssi_count_fields,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static struct RxGainControlFields get_default_lbt_rx_analog_configs(void)
{
    // These are the max E510 gain settings
    return (struct RxGainControlFields){.rx_atten   = RxAttenAtten_12_dB,
                                        .pga1_gain  = Pga1GainGain_12_dB,
                                        .pga2_gain  = Pga2GainGain_18_dB,
                                        .pga3_gain  = Pga3GainGain_18_dB,
                                        .Reserved0  = 0u,
                                        .mixer_gain = MixerGainGain_20p7_dB,
                                        .pga1_rin_select = false,
                                        .Reserved1       = 0u,
                                        .mixer_bandwidth = true};
}

static void run_listen_before_talk(uint8_t  r_divider_index,
                                   uint16_t n_divider,
                                   int32_t  offset_frequency_khz,
                                   uint8_t  rssi_count)
{
    assert(op_variables.ex10_protocol);

    struct RfSynthesizerControlFields const synth_control = {
        .n_divider = n_divider, .r_divider = r_divider_index, .lf_type = 1u};

    // For the RSSI measurement, we need to set the RSSI count register
    struct MeasureRssiCountFields const rssi_count_fields = {.samples =
                                                                 rssi_count};
    // We set the LBT offset frequency for the offset at which to measure RSSI
    struct LbtOffsetFields const  lbt_offset = {.khz = offset_frequency_khz};
    struct OpsControlFields const ops_control_data = {.op_id =
                                                          ListenBeforeTalkOp};

    struct RegisterInfo const* const regs[] = {
        &lbt_offset_reg,
        &rf_synthesizer_control_reg,
        &measure_rssi_count_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &lbt_offset,
        &synth_control,
        &rssi_count_fields,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void start_timer_op(uint32_t delay_us)
{
    assert(op_variables.ex10_protocol);

    struct DelayUsFields          delay_fields     = {.delay = delay_us};
    struct OpsControlFields const ops_control_data = {.op_id = UsTimerStartOp};

    struct RegisterInfo const* const regs[] = {
        &delay_us_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &delay_fields,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void wait_timer_op(void)
{
    assert(op_variables.ex10_protocol);

    struct OpsControlFields const ops_control_data = {.op_id = UsTimerWaitOp};
    op_variables.ex10_protocol->write(&ops_control_reg, &ops_control_data);
}

static void lock_synthesizer(uint8_t r_divider_index, uint16_t n_divider)
{
    struct RfSynthesizerControlFields const synth_control = {
        .n_divider = n_divider, .r_divider = r_divider_index, .lf_type = 1u};
    struct OpsControlFields const ops_control_data = {.op_id =
                                                          LockSynthesizerOp};

    struct RegisterInfo const* const regs[] = {
        &rf_synthesizer_control_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &synth_control,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void start_event_fifo_test(uint32_t period, uint8_t num_words)
{
    assert(op_variables.ex10_protocol);
    struct EventFifoTestPeriodFields const test_period = {.period = period};
    op_variables.ex10_protocol->write(&event_fifo_test_period_reg,
                                      &test_period);

    struct EventFifoTestPayloadNumWordsFields const payload_words = {
        .num_words = num_words};
    struct OpsControlFields const ops_control_data = {.op_id = EventFifoTestOp};

    struct RegisterInfo const* const regs[] = {
        &event_fifo_test_payload_num_words_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {
        &payload_words,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void insert_fifo_event(const bool                    trigger_irq,
                              const struct EventFifoPacket* event_packet)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->insert_fifo_event(trigger_irq, event_packet);
}

static void enable_sdd_logs(const struct LogEnablesFields enables,
                            const uint8_t                 speed_mhz)
{
    assert(op_variables.ex10_protocol);
    struct RegisterInfo const* const regs[] = {
        &log_enables_reg,
        &log_speed_reg,
    };

    struct LogSpeedFields log_speed = {.speed_mhz = speed_mhz, .rfu = 0u};

    void const* buffers[] = {
        &enables,
        &log_speed,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void send_gen2_halted_sequence(void)
{
    assert(op_variables.ex10_protocol);
    struct HaltedControlFields halted_controls = {
        .go = true, .resume = false, .nak_tag = false};
    op_variables.ex10_protocol->write(&halted_control_reg, &halted_controls);
}

static void continue_from_halted(bool nak)
{
    assert(op_variables.ex10_protocol);
    struct HaltedControlFields halted_controls = {
        .go = false, .resume = true, .nak_tag = nak};
    op_variables.ex10_protocol->write(&halted_control_reg, &halted_controls);
}

static struct OpCompletionStatus run_sjc(void)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->start_op(RxRunSjcOp);

    // Check op completion status and update local variable regardless of
    // outcome.
    struct OpCompletionStatus op_error = wait_op_completion();

    // Read back the analog rx settings in case the RxAtten value changed.
    // Store the results in the local variable stored_analog_rx_fields.
    op_variables.ex10_protocol->read(&rx_gain_control_reg,
                                     &op_variables.stored_analog_rx_fields);

    return op_error;
}

static struct Ex10SjcAccessor const* get_sjc(void)
{
    return op_variables.sjc_instance;
}

static void stop_op(void)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->stop_op();
}

static uint32_t read_gpio_output_enables(void)
{
    assert(op_variables.ex10_protocol);
    uint32_t output_enable = 0u;
    op_variables.ex10_protocol->read(&gpio_output_enable_reg, &output_enable);
    return output_enable;
}

static uint32_t read_gpio_output_levels(void)
{
    assert(op_variables.ex10_protocol);
    uint32_t output_level = 0u;
    op_variables.ex10_protocol->read(&gpio_output_level_reg, &output_level);
    return output_level;
}

static uint32_t get_device_time(void)
{
    assert(op_variables.ex10_protocol);
    uint32_t dev_time = 0u;
    op_variables.ex10_protocol->read(&timestamp_reg, &dev_time);
    return dev_time;
}

static struct GpioControlFields get_gpio(void)
{
    struct GpioControlFields const gpio_control = {
        .output_enable = read_gpio_output_enables(),
        .output_level  = read_gpio_output_levels()};

    return gpio_control;
}

static void set_gpio(uint32_t gpio_levels, uint32_t gpio_enables)
{
    struct RegisterInfo const* const regs[] = {
        &gpio_output_enable_reg,
        &gpio_output_level_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {.op_id = SetGpioOp};

    void const* buffers[] = {
        &gpio_enables,
        &gpio_levels,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(regs[0]));
}

static void set_clear_gpio_pins(
    struct GpioPinsSetClear const* gpio_pins_set_clear)
{
    struct RegisterInfo const* const regs[] = {
        &gpio_output_level_set_reg,
        &gpio_output_level_clear_reg,
        &gpio_output_enable_set_reg,
        &gpio_output_enable_clear_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {
        .op_id = SetClearGpioPinsOp,
    };

    void const* buffers[] = {
        &gpio_pins_set_clear->output_level_set,
        &gpio_pins_set_clear->output_level_clear,
        &gpio_pins_set_clear->output_enable_set,
        &gpio_pins_set_clear->output_enable_clear,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(regs[0]));
}

static void start_inventory_round(
    struct InventoryRoundControlFields const*   configs,
    struct InventoryRoundControl_2Fields const* configs_2)
{
    tracepoint(pi_ex10sdk, OPS_start_inventory_round, configs, configs_2);

    struct RegisterInfo const* const regs[] = {
        &inventory_round_control_reg,
        &inventory_round_control_2_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {
        .op_id = StartInventoryRoundOp};

    void const* buffers[] = {
        configs,
        configs_2,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void start_prbs(void)
{
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->start_op(RunPrbsDataOp);
}

static void start_hpf_override_test_op(
    struct HpfOverrideSettingsFields const* hpf_settings)
{
    assert(op_variables.ex10_protocol);

    struct RegisterInfo const* const regs[] = {
        &hpf_override_settings_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {.op_id =
                                                          HpfOverrideTestOp};

    void const* buffers[] = {
        hpf_settings,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void start_ber_test(uint16_t num_bits,
                           uint16_t num_packets,
                           bool     delimiter_only)
{
    // Determine whether to use a delimiter only instead of a full query
    struct BerModeFields ber_mode = {.del_only_mode = delimiter_only};

    struct BerControlFields ber_control = {.num_bits    = num_bits,
                                           .num_packets = num_packets};

    struct RegisterInfo const* const regs[] = {
        &ber_mode_reg,
        &ber_control_reg,
        &ops_control_reg,
    };

    struct OpsControlFields const ops_control_data = {.op_id = BerTestOp};

    void const* buffers[] = {
        &ber_mode,
        &ber_control,
        &ops_control_data,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void send_select(void)
{
    tracepoint(pi_ex10sdk, OPS_send_select);

    // Send the select
    op_variables.ex10_protocol->start_op(SendSelectOp);
}

static void write_calibration_page(uint8_t const* data_ptr, size_t write_length)
{
    assert(data_ptr);
    assert(op_variables.ex10_protocol);
    op_variables.ex10_protocol->write_calibration_page(data_ptr, write_length);
}

static void cw_off(void)
{
    tracepoint(pi_ex10sdk, OPS_cw_off_manual);
    tx_ramp_down();
}

static void run_aggregate_op(void)
{
    // Start the op
    op_variables.ex10_protocol->start_op(AggregateOp);
}

static struct OpCompletionStatus ramp_transmit_power(
    struct PowerConfigs*           power_config,
    struct RegulatoryTimers const* timer_config)
{
    set_tx_coarse_gain(power_config->tx_atten);

    struct OpCompletionStatus op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    set_tx_fine_gain(power_config->tx_scalar);

    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    tx_ramp_up(power_config->dc_offset, timer_config);

    // Adc target 0 means we don't intend to run power control
    if (power_config->adc_target == 0)
    {
        return op_error;
    }

    // Registers used to configure the power control loop
    struct PowerControlLoopAuxAdcControlFields const adc_control = {
        .channel_enable_bits = power_config->power_detector_adc};
    struct PowerControlLoopGainDivisorFields const gain_divisor = {
        .gain_divisor = power_config->loop_gain_divisor};
    struct PowerControlLoopMaxIterationsFields const max_iterations = {
        .max_iterations = power_config->max_iterations};
    struct PowerControlLoopAdcTargetFields const adc_target = {
        .adc_target_value = power_config->adc_target};
    struct PowerControlLoopAdcThresholdsFields const adc_thresholds = {
        .loop_stop_threshold = power_config->loop_stop_threshold,
        .op_error_threshold  = power_config->op_error_threshold};
    struct OpsControlFields const ops_control_data = {.op_id =
                                                          PowerControlLoopOp};

    struct RegisterInfo const* const regs[] = {
        &power_control_loop_aux_adc_control_reg,
        &power_control_loop_gain_divisor_reg,
        &power_control_loop_max_iterations_reg,
        &power_control_loop_adc_target_reg,
        &power_control_loop_adc_thresholds_reg,
        &ops_control_reg,
    };

    void const* buffers[] = {&adc_control,
                             &gain_divisor,
                             &max_iterations,
                             &adc_target,
                             &adc_thresholds,
                             &ops_control_data};

    // Now that we are ready to go, ensure the previous tx ramp up is done
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Write the controls for the power control loop and start the op
    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
    return op_error;
}

static struct OpCompletionStatus buffer_overflow(
    struct OpCompletionStatus op_error)
{
    op_error.error_occurred            = true;
    op_error.aggregate_buffer_overflow = true;
    return op_error;
}

static bool is_equal_synth_control(struct RfSynthesizerControlFields const* s1,
                                   struct RfSynthesizerControlFields const* s2)
{
    return (s1->n_divider == s2->n_divider && s1->r_divider == s2->r_divider &&
            s1->lf_type == s2->lf_type);
}

static struct OpCompletionStatus cw_on(
    struct GpioPinsSetClear const*             gpio_controls,
    uint16_t                                   rf_mode,
    struct PowerConfigs*                       power_config,
    struct RfSynthesizerControlFields const*   synth_control,
    struct RegulatoryTimers const*             timer_config,
    struct PowerDroopCompensationFields const* droop_comp)
{
    struct OpCompletionStatus op_error = {.error_occurred = false,
                                          .ops_status     = {0},
                                          .command_error  = Success,
                                          .timeout_error  = false};

    uint8_t agg_data[aggregate_op_buffer_reg.length];
    memset(agg_data, 0, aggregate_op_buffer_reg.length);
    struct ByteSpan agg_buffer = {.data = agg_data, .length = 0};
    struct Ex10AggregateOpBuilder const* agg_builder =
        get_ex10_aggregate_op_builder();

    if (!agg_builder->append_set_rf_mode(rf_mode, &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    // Prevent redundant CW on
    if (get_cw_is_on())
    {
        // If cw is on, we may still want to change the rf mode
        agg_builder->append_exit_instruction(&agg_buffer);
        agg_builder->set_buffer(&agg_buffer);

        // Run the aggregate op and wait for completion
        run_aggregate_op();
        return wait_op_completion();
    }
    tracepoint(pi_ex10sdk,
               OPS_cw_on,
               gpio_controls,
               rf_mode,
               power_config,
               synth_control,
               timer_config);

    // We now know that CW will be turned on, and thus can run the pre CW on
    // callback function.
    if (op_variables.pre_rampup_callback != NULL)
    {
        if (!op_variables.pre_rampup_callback())
        {
            // Note, if there are any issues in the callback, appropriate error
            // handling should happen there. The callback has the  context to
            // perform actions befitting the error. Meanwhile we will mark an
            // error occurred here for the user.
            op_error.error_occurred = true;
            return op_error;
        }
    }

    // If staying on the same channel, ensure off time.
    bool const ramp_same_channel = is_equal_synth_control(
        synth_control, &op_variables.stored_synth_control);

    if (ramp_same_channel)
    {
        // The timer is started here if needed and checked for completion after
        // everything else runs.
        if (!agg_builder->append_start_timer_op(
                timer_config->off_same_channel * 1000, &agg_buffer))
        {
            return buffer_overflow(op_error);
        }
    }
    else
    {
        op_variables.stored_synth_control = *synth_control;
    }

    if (!agg_builder->append_set_clear_gpio_pins(gpio_controls, &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    if (!agg_builder->append_lock_synthesizer(
            synth_control->r_divider, synth_control->n_divider, &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    // If the coarse gain has changes, store the new val and run the op
    if (!agg_builder->append_set_tx_coarse_gain(power_config->tx_atten,
                                                &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    if (!agg_builder->append_set_tx_fine_gain(power_config->tx_scalar,
                                              &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    if (!agg_builder->append_set_regulatory_timers(timer_config, &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    if (!agg_builder->append_droop_compensation(droop_comp, &agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    // If we started a timer due to ramping to the same channel, wait for the
    // timer to expire.
    if (ramp_same_channel)
    {
        if (!agg_builder->append_wait_timer_op(&agg_buffer))
        {
            return buffer_overflow(op_error);
        }
    }

    if (!agg_builder->append_tx_ramp_up(power_config->dc_offset, &agg_buffer) ||
        !agg_builder->append_power_control(power_config, &agg_buffer) ||
        !agg_builder->append_run_sjc(&agg_buffer))
    {
        return buffer_overflow(op_error);
    }

    // Add the exit instruction and set the buffer
    agg_builder->append_exit_instruction(&agg_buffer);
    agg_builder->set_buffer(&agg_buffer);

    // Run the aggregate op and wait for completion
    run_aggregate_op();
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Since the aggregate op ran sjc, we read back the analog rx settings
    // to check the RxAten.
    // Store the results in the local variable stored_analog_rx_fields.
    op_variables.ex10_protocol->read(&rx_gain_control_reg,
                                     &op_variables.stored_analog_rx_fields);

    // Updating to the next channel for the next CwOn
    op_variables.region->update_active_channel();

    // Done with all CW ramp activites, so run any post ramp functionality.
    if (op_variables.post_rampup_callback != NULL)
    {
        if (!op_variables.post_rampup_callback())
        {
            // Note, if there are any issues in the callback, appropriate error
            // handling should happen there. The callback has the  context to
            // perform actions befitting the error. Meanwhile we will mark an
            // error occurred here for the user.
            op_error.error_occurred = true;
            return op_error;
        }
    }
    return op_error;
}

static struct OpCompletionStatus inventory(
    struct GpioPinsSetClear const*              gpio_pins_set_clear,
    uint16_t                                    rf_mode,
    struct PowerConfigs*                        power_config,
    struct RfSynthesizerControlFields const*    synth_control,
    struct RegulatoryTimers const*              timer_config,
    struct InventoryRoundControlFields const*   inventory_config,
    struct InventoryRoundControl_2Fields const* inventory_config_2,
    bool                                        run_select_op,
    struct PowerDroopCompensationFields const*  droop_comp)
{
    struct OpCompletionStatus op_error = {.error_occurred = false,
                                          .ops_status     = {0},
                                          .command_error  = Success,
                                          .timeout_error  = false};

    // check to make sure that an op isn't running (say if inventory is
    // called twice by accident)
    assert(op_variables.ex10_protocol);
    if (op_variables.ex10_protocol->is_op_currently_running())
    {
        return op_error;
    }

    op_error = cw_on(gpio_pins_set_clear,
                     rf_mode,
                     power_config,
                     synth_control,
                     timer_config,
                     droop_comp);
    if (op_error.error_occurred)
    {
        return op_error;
    }
    // Ensure all ops done before sending a select
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    if (run_select_op)
    {
        // Sends the select in the gen2 tx buffer
        send_select();
        op_error = wait_op_completion();
        if (op_error.error_occurred)
        {
            return op_error;
        }
    }

    // Run a round of inventory and return even if CW is still on
    start_inventory_round(inventory_config, inventory_config_2);
    return op_error;
}

static struct OpCompletionStatus start_etsi_burst(
    struct InventoryRoundControlFields const*   inventory_config,
    struct InventoryRoundControl_2Fields const* inventory_config_2,
    struct GpioPinsSetClear const*              gpio_pins_set_clear,
    uint16_t                                    rf_mode,
    struct PowerConfigs*                        power_config,
    struct RfSynthesizerControlFields const*    synth_control,
    struct RegulatoryTimers const*              timer_config,
    uint16_t                                    on_time_ms,
    uint16_t                                    off_time_ms)
{
    struct OpCompletionStatus op_error = {.error_occurred = false,
                                          .ops_status     = {0},
                                          .command_error  = Success,
                                          .timeout_error  = false};

    // Prevent redundant CW on
    if (get_cw_is_on())
    {
        return op_error;
    }

    set_clear_gpio_pins(gpio_pins_set_clear);
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    lock_synthesizer(synth_control->r_divider, synth_control->n_divider);
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    set_rf_mode(rf_mode);
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Ramp up once to run the power control loop
    op_error = ramp_transmit_power(power_config, timer_config);
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // The CW on process used here waits for power control to
    // finish. This process ignores if the power ramp failed
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        if (op_error.ops_status.error != ErrorNone)
        {
            return op_error;
        }
        if (op_error.command_error != Success ||
            op_error.timeout_error != NoTimeout)
        {
            return op_error;
        }
    }

    // Ramp down and prepare for the etsi burst
    tx_ramp_down();
    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Set the timers for proper etsi burst
    struct RegulatoryTimers const etsi_burst_timers = {
        .nominal          = on_time_ms,
        .extended         = on_time_ms + 5,
        .regulatory       = 0,
        .off_same_channel = off_time_ms,
    };
    set_regulatory_timers(&etsi_burst_timers);

    // Set the inventory params for etsi burst
    op_variables.ex10_protocol->write(&inventory_round_control_reg,
                                      inventory_config);
    op_variables.ex10_protocol->write(&inventory_round_control_2_reg,
                                      inventory_config_2);

    op_error = wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Start the op
    op_variables.ex10_protocol->start_op(EtsiBurstOp);

    return op_error;
}

static void enable_droop_compensation(
    struct PowerDroopCompensationFields const* compensation)
{
    // Limit .interval and .step since keeping a relatively short interval
    // and small step size keeps Tx power changes within regulatory bounds.
    const uint8_t min_interval = 10u;
    const uint8_t max_interval = 40u;
    assert(compensation->compensation_interval_ms >= min_interval &&
           compensation->compensation_interval_ms <= max_interval);

    const uint8_t min_step = 5u;
    const uint8_t max_step = 15u;
    assert(compensation->fine_gain_step_cd_b >= min_step &&
           compensation->fine_gain_step_cd_b <= max_step);

    struct RegisterInfo const* const regs[] = {
        &power_droop_compensation_reg,
    };
    void const* buffers[] = {
        compensation,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static void disable_droop_compensation(void)
{
    struct PowerDroopCompensationFields compensation;
    op_variables.ex10_protocol->read(&power_droop_compensation_reg,
                                     &compensation);
    compensation.enable = false;

    struct RegisterInfo const* const regs[] = {
        &power_droop_compensation_reg,
    };
    void const* buffers[] = {
        &compensation,
    };

    op_variables.ex10_protocol->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));
}

static struct Ex10Ops const ex10_ops = {
    .init                              = init,
    .init_ex10                         = init_ex10,
    .release                           = release,
    .register_fifo_data_callback       = register_fifo_data_callback,
    .register_interrupt_callback       = register_interrupt_callback,
    .unregister_fifo_data_callback     = unregister_fifo_data_callback,
    .unregister_interrupt_callback     = unregister_interrupt_callback,
    .ex10_protocol                     = ex10_protocol,
    .run_aggregate_op                  = run_aggregate_op,
    .get_cw_is_on                      = get_cw_is_on,
    .read_ops_status                   = read_ops_status,
    .start_log_test                    = start_log_test,
    .set_atest_mux                     = set_atest_mux,
    .measure_aux_adc                   = measure_aux_adc,
    .set_aux_dac                       = set_aux_dac,
    .set_rf_mode                       = set_rf_mode,
    .rf_mode_is_drm                    = rf_mode_is_drm,
    .tx_ramp_up                        = tx_ramp_up,
    .tx_ramp_down                      = tx_ramp_down,
    .set_tx_coarse_gain                = set_tx_coarse_gain,
    .set_tx_fine_gain                  = set_tx_fine_gain,
    .radio_power_control               = radio_power_control,
    .set_analog_rx_config              = set_analog_rx_config,
    .measure_rssi                      = measure_rssi,
    .get_default_lbt_rx_analog_configs = get_default_lbt_rx_analog_configs,
    .run_listen_before_talk            = run_listen_before_talk,
    .start_timer_op                    = start_timer_op,
    .wait_timer_op                     = wait_timer_op,
    .lock_synthesizer                  = lock_synthesizer,
    .start_event_fifo_test             = start_event_fifo_test,
    .insert_fifo_event                 = insert_fifo_event,
    .enable_sdd_logs                   = enable_sdd_logs,
    .send_gen2_halted_sequence         = send_gen2_halted_sequence,
    .continue_from_halted              = continue_from_halted,
    .run_sjc                           = run_sjc,
    .get_sjc                           = get_sjc,
    .wait_op_completion                = wait_op_completion,
    .wait_op_completion_with_timeout   = wait_op_completion_with_timeout,
    .stop_op                           = stop_op,
    .get_gpio                          = get_gpio,
    .set_gpio                          = set_gpio,
    .set_clear_gpio_pins               = set_clear_gpio_pins,
    .start_inventory_round             = start_inventory_round,
    .start_prbs                        = start_prbs,
    .start_hpf_override_test_op        = start_hpf_override_test_op,
    .start_etsi_burst                  = start_etsi_burst,
    .start_ber_test                    = start_ber_test,
    .send_select                       = send_select,
    .write_calibration_page            = write_calibration_page,
    .cw_off                            = cw_off,
    .cw_on                             = cw_on,
    .ramp_transmit_power               = ramp_transmit_power,
    .inventory                         = inventory,
    .get_device_time                   = get_device_time,
    .get_current_analog_rx_config      = get_current_analog_rx_config,
    .enable_droop_compensation         = enable_droop_compensation,
    .disable_droop_compensation        = disable_droop_compensation,
    .register_pre_rampup_callback      = register_pre_rampup_callback,
    .register_post_rampup_callback     = register_post_rampup_callback,
    .unregister_pre_rampup_callback    = unregister_pre_rampup_callback,
    .unregister_post_rampup_callback   = unregister_post_rampup_callback,
    .op_error_none                     = &op_error_none,
};

struct Ex10Ops const* get_ex10_ops(void)
{
    return &ex10_ops;
}
