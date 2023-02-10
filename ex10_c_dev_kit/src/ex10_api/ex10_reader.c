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
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "board/board_spec.h"
#include "board/ex10_gpio.h"
#include "board/region.h"
#include "calibration.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/byte_span.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_ops.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/fifo_buffer_list.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/linked_list.h"
#include "ex10_api/regions_table.h"
#include "ex10_api/trace.h"
#include "ex10_api/version_info.h"


/**
 * @struct InventoryParams
 * These parameters are set via host calls throught the Ex10Reader interface.
 * They are never (and must never be) set from the fifo_data_handler thread.
 */
struct InventoryParams
{
    uint8_t                              antenna;
    uint16_t                             rf_mode;
    uint16_t                             tx_power_cdbm;
    struct InventoryRoundControlFields   inventory_config;
    struct InventoryRoundControl_2Fields inventory_config_2;
    bool                                 send_selects;
    struct StopConditions                stop_conditions;
    bool                                 dual_target;
    uint32_t                             frequency_khz;
    bool                                 remain_on;
    uint32_t                             start_time_us;
    uint8_t                              tag_focus_target;
};

struct ReversePowerParams
{
    uint16_t tx_power_cdbm;
    uint32_t frequency_khz;
    uint32_t return_loss_cdb;
    int16_t  max_margin;
};

static struct EventFifoPacket const invalid_event_packet = {
    .packet_type         = InvalidPacket,
    .us_counter          = 0u,
    .static_data         = NULL,
    .static_data_length  = 0u,
    .dynamic_data        = NULL,
    .dynamic_data_length = 0u,
    .is_valid            = false,
};

/**
 * @struct Ex10ReaderPrivate
 * Ex10Reader private state variables.
 */
struct Ex10ReaderPrivate
{
    /// Guards access to the fifo_buffer_list within this same structure.
    pthread_mutex_t               list_mutex;
    struct Ex10Ops const*         ops;
    struct Ex10EventParser const* event_parser;
    struct FifoBufferList const*  fifo_buffer_list;
    struct Ex10Calibration const* calibration;
    struct Ex10Region const*      region;
    uint16_t                      last_adc_temperature;
    struct Ex10LinkedList         event_fifo_list;
    struct ConstByteSpan          event_packets_iterator;
    struct EventFifoPacket        event_packet;
    struct InventoryParams        inventory_params;
    struct ReversePowerParams     rev_power_params;
    struct ContinuousInventoryState volatile inventory_state;
};

static struct Ex10ReaderPrivate reader = {
    .list_mutex             = PTHREAD_MUTEX_INITIALIZER,
    .ops                    = NULL,
    .event_parser           = NULL,
    .fifo_buffer_list       = NULL,
    .calibration            = NULL,
    .region                 = NULL,
    .last_adc_temperature   = 0u,
    .event_packets_iterator = {.data = NULL, .length = 0u},
    .inventory_params =
        {
            .antenna       = 0u,
            .rf_mode       = 0u,
            .tx_power_cdbm = 0u,
            .send_selects  = false,
            .stop_conditions =
                {
                    .max_number_of_rounds = 0u,
                    .max_number_of_tags   = 0u,
                    .max_duration_us      = 0u,
                },
            .dual_target      = false,
            .frequency_khz    = 0u,
            .remain_on        = false,
            .start_time_us    = 0u,
            .tag_focus_target = SELECT_TARGET_MAX,
        },
    // Default parameters for reverse power threshold as used by the reader
    // callbacks.
    .rev_power_params =
        {
            .tx_power_cdbm   = 0,
            .frequency_khz   = 0,
            .return_loss_cdb = 1000,
            .max_margin      = -200,
        },
    .inventory_state =
        {
            .stop_reason                   = SRNone,
            .state                         = InvIdle,
            .round_count                   = 0u,
            .tag_count                     = 0u,
            .target                        = 0u,
            .previous_q                    = 0u,
            .min_q_count                   = 0u,
            .queries_since_valid_epc_count = 0u,
            .done_reason                   = 0u,
        },
};

/// Tx power droop compensation with 25ms interval and .01dB step.
static struct PowerDroopCompensationFields droop_comp_defaults = {
    .enable                   = true,
    .compensation_interval_ms = 25,
    .fine_gain_step_cd_b      = 10,
};

/* Forward declarations */
static void fifo_data_handler(struct FifoBufferNode* fifo_buffer);
static bool interrupt_handler(struct InterruptStatusFields irq_status);
static void insert_fifo_event(const bool                    trigger_irq,
                              struct EventFifoPacket const* event_packet);
static bool post_rampup_functionality(void);
static struct OpCompletionStatus stop_transmitting(void);
static bool get_return_loss_threshold_exceeded(uint16_t return_loss_cdb,
                                               uint16_t tx_power_cdbm,
                                               uint32_t frequency_khz,
                                               int16_t  max_margin);
static struct OpCompletionStatus inventory(
    uint8_t                                     antenna,
    uint16_t                                    rf_mode,
    uint16_t                                    tx_power_cdbm,
    struct InventoryRoundControlFields const*   inventory_config,
    struct InventoryRoundControl_2Fields const* inventory_config_2,
    bool                                        send_selects,
    uint32_t                                    frequency_khz,
    bool                                        remain_on);

static void init(struct Ex10Ops const* ex10_ops, char const* region_name)
{
    list_init(&reader.event_fifo_list);
    reader.event_packets_iterator.data   = NULL;
    reader.event_packets_iterator.length = 0u;
    reader.event_packet                  = invalid_event_packet;

    reader.region = get_ex10_region();
    reader.region->init(region_name, TCXO_FREQ_KHZ);

    reader.ops              = ex10_ops;
    reader.event_parser     = get_ex10_event_parser();
    reader.fifo_buffer_list = get_ex10_fifo_buffer_list();
    reader.calibration      = NULL;  // see Ex10Reader.read_calibration()

    reader.inventory_state.state = InvIdle;

    reader.ops->unregister_fifo_data_callback();
    reader.ops->register_fifo_data_callback(fifo_data_handler);
}

/**
 * @details
 * Error handling: it is best to work through as much of this initialization
 * as possible and return an error code at the end, thereby setting up the
 * reader state variables into as consistent a form possible.
 */
static struct OpCompletionStatus init_ex10(void)
{
    reader.ops->unregister_interrupt_callback();

    // Note: Ex10Ops.init_ex10() clears the interrupt mask.
    // Therefore, Ex10Reader.init_ex10() should be called after
    // Ex10Ops.init_ex10() in board initialization ex10_typical_board_setup().
    struct InterruptMaskFields const register_int_mask = {
        .op_done                 = true,
        .halted                  = true,
        .event_fifo_above_thresh = true,
        .event_fifo_full         = true,
        .inventory_round_done    = true,
        .halted_sequence_done    = true,
        .command_error           = true,
        .aggregate_op_done       = false,
    };
    reader.ops->register_interrupt_callback(register_int_mask,
                                            interrupt_handler);

    // Enable the Ex10 analog power supplies by running the RadioPowerControlOp.
    reader.ops->radio_power_control(true);
    struct OpCompletionStatus const op_error_radio_power_control =
        reader.ops->wait_op_completion();

    // Set the GPIO initial levels and enables to the value specified in the
    // board layer.
    uint32_t const gpio_reserved_mask =
        (1u << GPIO_PIN_IRQ_N) | (1u << GPIO_PIN_SDD_CS_N);
    uint32_t const gpio_allowed_mask = ~gpio_reserved_mask;

    uint32_t const gpio_output_enable_set =
        get_ex10_board_spec()->get_gpio_output_enables();
    uint32_t const gpio_output_enable_clear = ~gpio_output_enable_set;

    uint32_t const gpio_output_level_set =
        get_ex10_board_spec()->get_default_gpio_output_levels();
    uint32_t const gpio_output_level_clear = ~gpio_output_level_set;

    struct GpioPinsSetClear const gpio_pins_set_clear = {
        .output_level_set    = gpio_output_level_set & gpio_allowed_mask,
        .output_level_clear  = gpio_output_level_clear & gpio_allowed_mask,
        .output_enable_set   = gpio_output_enable_set & gpio_allowed_mask,
        .output_enable_clear = gpio_output_enable_clear & gpio_allowed_mask,
    };

    reader.ops->set_clear_gpio_pins(&gpio_pins_set_clear);
    struct OpCompletionStatus const op_error_gpio =
        reader.ops->wait_op_completion();

    // Prepare the buffer for gen2 commands
    get_ex10_gen2_tx_command_manager()->init();
    // Set the value to be above the max possible target
    reader.inventory_params.tag_focus_target = SELECT_TARGET_MAX;

    // Register the reader post ramp up callback to occur after cw_on calls
    reader.ops->unregister_post_rampup_callback();
    reader.ops->register_post_rampup_callback(post_rampup_functionality);

    reader.inventory_state.state = InvIdle;

    if (op_error_radio_power_control.error_occurred)
    {
        return op_error_radio_power_control;
    }
    return op_error_gpio;
}

static void read_calibration(void)
{
    reader.calibration = get_ex10_calibration(reader.ops->ex10_protocol());
    reader.calibration->init(reader.ops->ex10_protocol());
}

static bool post_rampup_functionality(void)
{
    // Call the reader function to check for proper rx power detection
    // Note: The tx power and frequency requires use of the reader layer to
    // update the stored values.
    const bool thresh_exceeded = get_return_loss_threshold_exceeded(
        reader.rev_power_params.return_loss_cdb,
        reader.rev_power_params.tx_power_cdbm,
        reader.rev_power_params.frequency_khz,
        reader.rev_power_params.max_margin);

    // Problem in rx path, so stop transmitting
    if (thresh_exceeded)
    {
        stop_transmitting();
    }
    // thresh exceed is true if there is an issue. This callback uses a return
    // of false to signify error
    return !thresh_exceeded;
}

static void deinit(void)
{
    // Don't rely on init() being called.
    // Ex10Reader might not have been initialized; i.e. bootloader startup.
    struct Ex10Protocol const* ex10_protocol = get_ex10_protocol();
    ex10_protocol->unregister_interrupt_callback();
    ex10_protocol->unregister_fifo_data_callback();
}

/**
 * Push a FifoBufferNode onto the back of the reader.event_fifo_list.
 * The reader list mutex is used to maintain exclusive access to the
 * reader list.
 *
 * @param fifo_buffer_node A pointer to the FifoBufferNode.
 */
static void reader_list_node_push_back(struct FifoBufferNode* fifo_buffer_node)
{
    pthread_mutex_lock(&reader.list_mutex);
    list_push_back(&reader.event_fifo_list, &fifo_buffer_node->list_node);
    pthread_mutex_unlock(&reader.list_mutex);
}

/**
 * Pop a FifoBufferNode from the front of the reader.event_fifo_list.
 * The reader list mutex is used to maintain exclusive access to the
 * reader list. If the reader list is empty, the NULL is returned.
 *
 * @return struct FifoBufferNode* A pointer to the front FifoBufferNode on the
 *                                reader list.
 * @retval NULL If there are no free nodes in the reader list.
 */
static struct FifoBufferNode* reader_list_node_pop_front(void)
{
    struct FifoBufferNode* fifo_buffer_node = NULL;

    pthread_mutex_lock(&reader.list_mutex);
    struct Ex10ListNode* list_node = list_front(&reader.event_fifo_list);

    // Note: if list_node->data ==  NULL, then the reader list is empty.
    // i.e. list_node is pointing to the sentinal node.
    if (list_node->data)
    {
        list_pop_front(&reader.event_fifo_list);
        fifo_buffer_node = (struct FifoBufferNode*)list_node->data;
    }
    pthread_mutex_unlock(&reader.list_mutex);

    return fifo_buffer_node;
}

static struct FifoBufferNode const* event_fifo_buffer_peek(void)
{
    // If the node is a sentinal node then list_node->data will be NULL
    // indicating that there are no buffers in the reader list.
    struct Ex10ListNode* list_node = list_front(&reader.event_fifo_list);
    if (list_node)
    {
        return (struct FifoBufferNode const*)list_node->data;
    }
    else
    {
        return NULL;
    }
}

static void event_fifo_buffer_pop(void)
{
    struct FifoBufferNode* fifo_buffer_node = reader_list_node_pop_front();

    bool free_list_is_empty = false;
    if (fifo_buffer_node)
    {
        free_list_is_empty =
            reader.fifo_buffer_list->free_list_put(fifo_buffer_node);
    }

    if (free_list_is_empty)
    {
        // The buffer queue for the interrupt to obtain packets from
        // was empty. Initiate an interrupt from Ex10 to get the ReadFifo
        // command restarted.
        insert_fifo_event(true, NULL);
    }
}

static void parse_next_event_fifo_packet(void)
{
    if (reader.event_packets_iterator.length == 0u)
    {
        // The iterator has reached the end of a buffer node or
        // it is not pointing to a node.
        if (reader.event_packets_iterator.data != NULL)
        {
            // The event buffer ConstByteSpan data pointer was pointing
            // to the end of non-null.
            // Free the current FifoBufferNode in the list.
            event_fifo_buffer_pop();

            // Invalidate the ConstByteSpan data pointer to indicate that
            // there no current EventFifo data buffer being parsed.
            reader.event_packets_iterator.data = NULL;
        }

        struct FifoBufferNode const* fifo_buffer = event_fifo_buffer_peek();
        if (fifo_buffer != NULL)
        {
            reader.event_packets_iterator = fifo_buffer->fifo_data;
        }
        else
        {
            // There were no FifoBufferNode elements in the reader list.
            reader.event_packets_iterator.data   = NULL;
            reader.event_packets_iterator.length = 0u;
        }
    }

    if (reader.event_packets_iterator.length > 0u)
    {
        reader.event_packet = reader.event_parser->parse_event_packet(
            &reader.event_packets_iterator);
    }
    else
    {
        reader.event_packet = invalid_event_packet;
    }
}

static void push_continuous_inventory_summary_packet(
    struct EventFifoPacket const* event_packet,
    struct OpCompletionStatus     op_error)
{
    uint32_t const duration_us =
        event_packet->us_counter - reader.inventory_params.start_time_us;

    struct ContinuousInventorySummary summary = {
        .duration_us                = duration_us,
        .number_of_inventory_rounds = reader.inventory_state.round_count,
        .number_of_tags             = reader.inventory_state.tag_count,
        .reason                     = reader.inventory_state.stop_reason,
        .last_op_id                 = 0u,
        .last_op_error              = ErrorNone,
        .packet_rfu_1               = 0u,
    };

    // Update with any SDK errors encountered
    if (op_error.error_occurred)
    {
        if (op_error.timeout_error != NoTimeout)
        {
            summary.reason = SRSdkTimeoutError;
        }
        else if (op_error.command_error != Success)
        {
            summary.reason = SRDeviceCommandError;
        }
        else if (op_error.ops_status.error != ErrorNone)
        {
            summary.reason        = SROpError;
            summary.last_op_id    = op_error.ops_status.op_id;
            summary.last_op_error = op_error.ops_status.error;
        }
    }

    struct EventFifoPacket const summary_packet = {
        .packet_type         = ContinuousInventorySummary,
        .us_counter          = event_packet->us_counter,
        .static_data         = (union PacketData const*)&summary,
        .static_data_length  = sizeof(struct ContinuousInventorySummary),
        .dynamic_data        = NULL,
        .dynamic_data_length = 0u,
        .is_valid            = true,
    };

    bool const trigger_irq = true;
    insert_fifo_event(trigger_irq, &summary_packet);
}

static bool check_stop_conditions(uint32_t timestamp_us)
{
    // if the reason is already set, we return so as to retain the original stop
    // reason
    if (reader.inventory_state.stop_reason != SRNone)
    {
        return true;
    }

    if (reader.inventory_params.stop_conditions.max_number_of_rounds > 0u)
    {
        if (reader.inventory_state.round_count >=
            reader.inventory_params.stop_conditions.max_number_of_rounds)
        {
            reader.inventory_state.stop_reason = SRMaxNumberOfRounds;
            return true;
        }
    }
    if (reader.inventory_params.stop_conditions.max_number_of_tags > 0u)
    {
        if (reader.inventory_state.tag_count >=
            reader.inventory_params.stop_conditions.max_number_of_tags)
        {
            reader.inventory_state.stop_reason = SRMaxNumberOfTags;
            return true;
        }
    }
    if (reader.inventory_params.stop_conditions.max_duration_us > 0u)
    {
        // packet before start checks for packets which occurred before the
        // continuous inventory round was started.
        bool packet_before_start =
            reader.inventory_params.start_time_us > timestamp_us;
        uint32_t const elapsed_us =
            (packet_before_start)
                ? ((UINT32_MAX - reader.inventory_params.start_time_us) +
                   timestamp_us + 1)
                : (timestamp_us - reader.inventory_params.start_time_us);
        if (elapsed_us >=
            reader.inventory_params.stop_conditions.max_duration_us)
        {
            reader.inventory_state.stop_reason = SRMaxDuration;
            return true;
        }
    }
    if (reader.inventory_state.state == InvStopRequested)
    {
        reader.inventory_state.stop_reason = SRHost;
        return true;
    }
    return false;
}

static struct OpCompletionStatus continue_inventory(void)
{
    /* Behavior for stop reasons:
    InventorySummaryDone          // Flip target (dual target), reset Q
    InventorySummaryHost          // Don't care
    InventorySummaryRegulatory    // Preserve Q
    InventorySummaryEventFifoFull // Don't care
    InventorySummaryTxNotRampedUp // Don't care
    InventorySummaryInvalidParam  // Don't care
    InventorySummaryLmacOverload  // Don't care
    */

    bool reset_q = false;
    if (reader.inventory_params.dual_target)
    {
        // Flip target if round is done, not for regulatory or error.
        if (reader.inventory_state.done_reason == InventorySummaryDone)
        {
            reader.inventory_state.target ^= 1u;
            reset_q = true;
        }

        // If CW is not on and our session is one with
        // no persistence after power, we need to switch
        // the target to A
        if ((reader.ops->get_cw_is_on() == false) &&
            (reader.inventory_params.inventory_config.session == 0))
        {
            reset_q                       = true;
            reader.inventory_state.target = 0u;
        }
    }
    else
    {
        if (reader.inventory_state.done_reason == InventorySummaryDone)
        {
            reset_q = true;
        }
    }

    struct InventoryRoundControlFields inventory_config =
        reader.inventory_params.inventory_config;
    inventory_config.target = reader.inventory_state.target;

    struct InventoryRoundControl_2Fields inventory_config_2 =
        reader.inventory_params.inventory_config_2;

    // Preserve Q  and internal LMAC counters across rounds or reset for
    // new target.
    if (reset_q)
    {
        // Reset Q for target flip (done above) or for normal end of round.
        inventory_config.initial_q =
            reader.inventory_state.initial_inventory_config.initial_q;
        inventory_config_2.starting_min_q_count                       = 0;
        inventory_config_2.starting_max_queries_since_valid_epc_count = 0;
    }
    else if (reader.inventory_state.done_reason == InventorySummaryRegulatory)
    {
        // Preserve Q across rounds
        inventory_config.initial_q = reader.inventory_state.previous_q;
        inventory_config_2.starting_min_q_count =
            reader.inventory_state.min_q_count;
        inventory_config_2.starting_max_queries_since_valid_epc_count =
            reader.inventory_state.queries_since_valid_epc_count;
    }

    return inventory(reader.inventory_params.antenna,
                     reader.inventory_params.rf_mode,
                     reader.inventory_params.tx_power_cdbm,
                     &inventory_config,
                     &inventory_config_2,
                     reader.inventory_params.send_selects,
                     reader.inventory_params.frequency_khz,
                     reader.inventory_params.remain_on);
}

// Called by the interrupt handler thread when there is a non-fifo related
// interrupt.
static bool interrupt_handler(struct InterruptStatusFields irq_status)
{
    (void)irq_status;
    return true;
}


// Called by the interrupt handler thread when there is a fifo related
// interrupt.
static void fifo_data_handler(struct FifoBufferNode* fifo_buffer_node)
{
    struct ConstByteSpan bytes = fifo_buffer_node->fifo_data;
    while (bytes.length > 0u)
    {
        struct EventFifoPacket const packet =
            reader.event_parser->parse_event_packet(&bytes);
        if (reader.event_parser->get_packet_type_valid(packet.packet_type) ==
            false)
        {
            // Invalid packets cannot be processed and will only confuse the
            // continuous inventory state machine. Discontinue processing.
            fprintf(stderr,
                    "Invalid packet encountered during continuous inventory "
                    "packet parsing, packet end pos: %zu\n",
                    bytes.length);
            break;
        }
        if (reader.inventory_state.state != InvIdle)
        {
            if (packet.packet_type == InventoryRoundSummary)
            {
                reader.inventory_state.round_count += 1;

                // Save Q to use for next round's initial Q.
                if (packet.static_data->inventory_round_summary.reason ==
                    InventorySummaryRegulatory)
                {
                    reader.inventory_state.previous_q =
                        packet.static_data->inventory_round_summary.final_q;
                }

                reader.inventory_state.min_q_count =
                    packet.static_data->inventory_round_summary.min_q_count;
                reader.inventory_state.queries_since_valid_epc_count =
                    packet.static_data->inventory_round_summary
                        .queries_since_valid_epc_count;
                reader.inventory_state.done_reason =
                    packet.static_data->inventory_round_summary.reason;
            }
            else if (packet.packet_type == TagRead)
            {
                reader.inventory_state.tag_count += 1;
            }

            if (packet.packet_type == InventoryRoundSummary)
            {
                if (check_stop_conditions(packet.us_counter))
                {
                    struct OpsStatusFields ops_status;
                    memset(&ops_status, 0u, sizeof(ops_status));
                    struct OpCompletionStatus op_error = {
                        .error_occurred = false,
                        .ops_status     = ops_status,
                        .command_error  = Success,
                        .timeout_error  = NoTimeout};

                    reader.inventory_state.state = InvIdle;
                    push_continuous_inventory_summary_packet(&packet, op_error);
                }
                else
                {
                    struct OpCompletionStatus op_error = continue_inventory();
                    if (op_error.error_occurred)
                    {
                        reader.inventory_state.state = InvIdle;
                        push_continuous_inventory_summary_packet(&packet,
                                                                 op_error);
                    }
                }
            }
        }
    }

    // The FifoBufferNode must be placed into the reader list after the
    // continuous inventory state is updated within the IRQ_N monitor thread
    // context.
    reader_list_node_push_back(fifo_buffer_node);
}

static struct EventFifoPacket const* packet_peek(void)
{
    // If the packet iterator is NULL, attempt to get a fifo buffer from the
    // free list and parse the first packet.
    if (reader.event_packets_iterator.data == NULL)
    {
        parse_next_event_fifo_packet();
    }

    // If the packet buffer is not null, then the event_packet must have
    // been parsed (even if the packet was marked .is_value = false).
    if (reader.event_packets_iterator.data != NULL)
    {
        return &reader.event_packet;
    }

    // No packets have been parsed nor are any available for parsing.
    return NULL;
}

static void packet_remove(void)
{
    parse_next_event_fifo_packet();
}

static bool packets_available(void)
{
    return packet_peek();
}

static void continue_from_halted(bool nak)
{
    reader.ops->continue_from_halted(nak);
}

static struct RegulatoryTimers const regulatory_timers_disabled = {
    .nominal          = 0u,
    .extended         = 0u,
    .regulatory       = 0u,
    .off_same_channel = 0u};

static struct OpCompletionStatus build_cw_configs(uint8_t  antenna,
                                                  uint16_t rf_mode,
                                                  uint16_t tx_power_cdbm,
                                                  uint32_t frequency_khz,
                                                  bool     remain_on,
                                                  struct CwConfig* cw_config)
{
    uint16_t temperature_adc   = 0u;
    bool     temp_comp_enabled = true;

    struct OpsStatusFields ops_status;
    memset(&ops_status, 0u, sizeof(ops_status));
    struct OpCompletionStatus op_error = {.error_occurred = false,
                                          .ops_status     = ops_status,
                                          .command_error  = Success,
                                          .timeout_error  = NoTimeout};
    // If CW is already on, there is no need to measure temp for power settings.
    if (false == reader.ops->get_cw_is_on())
    {
        op_error = reader.ops->measure_aux_adc(
            AdcResultTemperature, 1u, &temperature_adc);
        if (op_error.error_occurred)
        {
            return op_error;
        }

        reader.last_adc_temperature = temperature_adc;
        // If the temperature value is invalid, disable the temperature
        // compensation
        if (temperature_adc > 500u)
        {
            temp_comp_enabled = false;
        }
    }
    else
    {
        temp_comp_enabled = false;
    }

    struct SynthesizerParams synth_params;
    memset(&synth_params, 0u, sizeof(synth_params));

    reader.region->get_synthesizer_params(frequency_khz, &synth_params);

    if (frequency_khz == 0u)
    {
        frequency_khz = synth_params.freq_khz;
    }
    // Cache the tx power and frequency for use in rx power threshold detection
    reader.rev_power_params.tx_power_cdbm = tx_power_cdbm;
    reader.rev_power_params.frequency_khz = frequency_khz;


    // If remain_on is set then disable the regulatory timers.
    struct RegulatoryTimers reg_timers = regulatory_timers_disabled;
    if (remain_on == false)
    {
        reader.region->get_regulatory_timers(&reg_timers);
    }

    bool const use_drm_filter = reader.ops->rf_mode_is_drm(rf_mode);
    enum BasebandFilterType const rx_baseband_filter =
        use_drm_filter ? BasebandFilterBandpass : BasebandFilterHighpass;

    get_ex10_board_spec()->get_gpio_output_pins_set_clear(
        &cw_config->gpio,
        antenna,
        tx_power_cdbm,
        rx_baseband_filter,
        reader.region->get_rf_filter());
    cw_config->rf_mode = rf_mode;

    cw_config->power = reader.calibration->get_power_control_params(
        tx_power_cdbm,
        frequency_khz,
        temperature_adc,
        temp_comp_enabled,
        reader.region->get_rf_filter());

    cw_config->synth.r_divider = synth_params.r_divider_index;
    cw_config->synth.n_divider = synth_params.n_divider;
    cw_config->synth.lf_type   = true;

    cw_config->timer.nominal          = reg_timers.nominal;
    cw_config->timer.extended         = reg_timers.extended;
    cw_config->timer.regulatory       = reg_timers.regulatory;
    cw_config->timer.off_same_channel = reg_timers.off_same_channel;

    return op_error;
}

static struct OpCompletionStatus inventory_continuous(
    uint8_t                                     antenna,
    uint16_t                                    rf_mode,
    uint16_t                                    tx_power_cdbm,
    struct InventoryRoundControlFields const*   inventory_config,
    struct InventoryRoundControl_2Fields const* inventory_config_2,
    bool                                        send_selects,
    struct StopConditions const*                stop_conditions,
    bool                                        dual_target,
    uint32_t                                    frequency_khz,
    bool                                        remain_on)
{
    // Ensure the proper configs were passed in.
    assert(stop_conditions);
    assert(inventory_config &&
           "The inventory config register is a required parameter");
    assert(inventory_config_2 &&
           "The inventory config 2 register is a required parameter");

    // Marking that we are in continuous inventory mode and reset all
    // config parameters.
    reader.inventory_state.state       = InvOngoing;
    reader.inventory_state.stop_reason = SRNone;
    reader.inventory_state.round_count = 0u;
    // Save initial inv params to reset Q on target flip.
    reader.inventory_state.initial_inventory_config      = *inventory_config;
    reader.inventory_state.previous_q                    = 0u;
    reader.inventory_state.min_q_count                   = 0u;
    reader.inventory_state.queries_since_valid_epc_count = 0u;
    reader.inventory_state.done_reason                   = 0u;
    reader.inventory_state.tag_count                     = 0u;
    reader.inventory_state.target = inventory_config->target;

    // Store passed in params
    reader.inventory_params.antenna            = antenna;
    reader.inventory_params.rf_mode            = rf_mode;
    reader.inventory_params.tx_power_cdbm      = tx_power_cdbm;
    reader.inventory_params.inventory_config   = *inventory_config;
    reader.inventory_params.inventory_config_2 = *inventory_config_2;

    // Check if a select config was passed in before making a copy.
    reader.inventory_params.send_selects = send_selects;

    reader.inventory_params.stop_conditions = *stop_conditions;
    reader.inventory_params.dual_target     = dual_target;

    reader.inventory_params.frequency_khz = frequency_khz;
    reader.inventory_params.remain_on     = remain_on;

    reader.inventory_params.start_time_us = reader.ops->get_device_time();

    // Begin inventory
    struct OpCompletionStatus op_error = inventory(antenna,
                                                   rf_mode,
                                                   tx_power_cdbm,
                                                   inventory_config,
                                                   inventory_config_2,
                                                   send_selects,
                                                   frequency_khz,
                                                   remain_on);
    if (op_error.error_occurred)
    {
        reader.inventory_state.state = InvIdle;
    }
    return op_error;
}

static struct OpCompletionStatus inventory(
    uint8_t                                     antenna,
    uint16_t                                    rf_mode,
    uint16_t                                    tx_power_cdbm,
    struct InventoryRoundControlFields const*   inventory_config,
    struct InventoryRoundControl_2Fields const* inventory_config_2,
    bool                                        send_selects,
    uint32_t                                    frequency_khz,
    bool                                        remain_on)
{
    struct CwConfig           cw_config;
    struct OpCompletionStatus op_error = build_cw_configs(
        antenna, rf_mode, tx_power_cdbm, frequency_khz, remain_on, &cw_config);
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Tag focus causes a tag to only reply once based on the SL flag
    // The tag therefore requires that the target is not constantly
    // switching while using tag focus.
    if (inventory_config->tag_focus_enable)
    {
        if (reader.inventory_params.tag_focus_target == SELECT_TARGET_MAX)
        {
            reader.inventory_params.tag_focus_target = inventory_config->target;
        }
        else
        {
            if (reader.inventory_params.tag_focus_target !=
                inventory_config->target)
            {
                op_error.error_occurred = true;
                fprintf(stderr,
                        "Target may not change while tag focus is enabled\n");
                return op_error;
            }
        }
    }
    else
    {
        reader.inventory_params.tag_focus_target = SELECT_TARGET_MAX;
    }

    // Cache the antenna and mode members of reader.inventory_params
    // for calculating RSSI compensation.
    // Generally, the reader.inventory_params is specific to continuous
    // inventory operation, which call this function to start the next
    // inventory once the inventory completes. When setting inventory_params
    // members, be sure that the values are consistent with the continuous
    // inventory operation.
    reader.inventory_params.antenna = antenna;
    reader.inventory_params.rf_mode = rf_mode;

    return reader.ops->inventory(&cw_config.gpio,
                                 cw_config.rf_mode,
                                 &cw_config.power,
                                 &cw_config.synth,
                                 &cw_config.timer,
                                 inventory_config,
                                 inventory_config_2,
                                 send_selects,
                                 &droop_comp_defaults);
}

static struct OpCompletionStatus cw_test(uint8_t  antenna,
                                         uint16_t rf_mode,
                                         uint16_t tx_power_cdbm,
                                         uint32_t frequency_khz,
                                         bool     remain_on)
{
    struct CwConfig           cw_config;
    struct OpCompletionStatus op_error = build_cw_configs(
        antenna, rf_mode, tx_power_cdbm, frequency_khz, remain_on, &cw_config);
    if (op_error.error_occurred)
    {
        return op_error;
    }

    return reader.ops->cw_on(&cw_config.gpio,
                             cw_config.rf_mode,
                             &cw_config.power,
                             &cw_config.synth,
                             &cw_config.timer,
                             &droop_comp_defaults);
}

static void write_cal_info_page(uint8_t const* data_ptr, size_t write_length)
{
    assert(data_ptr);

    reader.ops->write_calibration_page(data_ptr, write_length);
}

static struct OpCompletionStatus prbs_test(uint8_t  antenna,
                                           uint16_t rf_mode,
                                           uint16_t tx_power_cdbm,
                                           uint32_t frequency_khz,
                                           bool     remain_on)
{
    struct OpCompletionStatus op_error =
        cw_test(antenna, rf_mode, tx_power_cdbm, frequency_khz, remain_on);
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Wait until fully ramped up
    op_error = reader.ops->wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    reader.ops->start_prbs();
    return op_error;
}

static struct OpCompletionStatus ber_test(uint8_t  antenna,
                                          uint16_t rf_mode,
                                          uint16_t tx_power_cdbm,
                                          uint32_t frequency_khz,
                                          uint16_t num_bits,
                                          uint16_t num_packets,
                                          bool     delimiter_only)
{
    const bool                remain_on = true;
    struct OpCompletionStatus op_error =
        cw_test(antenna, rf_mode, tx_power_cdbm, frequency_khz, remain_on);
    if (op_error.error_occurred)
    {
        return op_error;
    }

    // Wait until fully ramped up
    op_error = reader.ops->wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    reader.ops->start_ber_test(num_bits, num_packets, delimiter_only);
    return op_error;
}

static struct OpCompletionStatus etsi_burst_test(
    struct InventoryRoundControlFields const*   inventory_config,
    struct InventoryRoundControl_2Fields const* inventory_config_2,
    uint8_t                                     antenna,
    uint16_t                                    rf_mode,
    uint16_t                                    tx_power_cdbm,
    uint16_t                                    on_time_ms,
    uint16_t                                    off_time_ms,
    uint32_t                                    frequency_khz)
{
    struct CwConfig           cw_config;
    struct OpCompletionStatus op_error = build_cw_configs(
        antenna, rf_mode, tx_power_cdbm, frequency_khz, false, &cw_config);
    if (op_error.error_occurred)
    {
        return op_error;
    }

    return reader.ops->start_etsi_burst(inventory_config,
                                        inventory_config_2,
                                        &cw_config.gpio,
                                        cw_config.rf_mode,
                                        &cw_config.power,
                                        &cw_config.synth,
                                        &cw_config.timer,
                                        on_time_ms,
                                        off_time_ms);
}

static void insert_fifo_event(const bool                    trigger_irq,
                              struct EventFifoPacket const* event_packet)
{
    reader.ops->insert_fifo_event(trigger_irq, event_packet);
}

static void enable_sdd_logs(const struct LogEnablesFields enables,
                            const uint8_t                 speed_mhz)
{
    reader.ops->enable_sdd_logs(enables, speed_mhz);
}

static struct OpCompletionStatus stop_transmitting(void)
{
    if (reader.inventory_state.state != InvIdle)
    {
        reader.inventory_state.state = InvStopRequested;
    }

    reader.ops->stop_op();
    struct OpCompletionStatus op_error = reader.ops->wait_op_completion();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    reader.ops->cw_off();
    return reader.ops->wait_op_completion();
}

static int16_t get_current_compensated_rssi(uint16_t rssi_raw)
{
    return reader.calibration->get_compensated_rssi(
        rssi_raw,
        reader.inventory_params.rf_mode,
        reader.ops->get_current_analog_rx_config(),
        reader.inventory_params.antenna,
        reader.region->get_rf_filter(),
        reader.last_adc_temperature);
}

static uint16_t get_current_rssi_log2(int16_t rssi_cdbm)
{
    return reader.calibration->get_rssi_log2(
        rssi_cdbm,
        reader.inventory_params.rf_mode,
        reader.ops->get_current_analog_rx_config(),
        reader.inventory_params.antenna,
        reader.region->get_rf_filter(),
        reader.last_adc_temperature);
}


static int16_t get_listen_before_talk_rssi(uint8_t  antenna,
                                           uint32_t frequency_khz,
                                           int32_t  lbt_offset,
                                           uint8_t  rssi_count,
                                           bool     override_used)
{
    struct SynthesizerParams synth_params;
    memset(&synth_params, 0u, sizeof(synth_params));
    reader.region->get_synthesizer_params(frequency_khz, &synth_params);

    // Tx power not used for LBT because it is a measurement when the
    // transmitter is off. The dummy measurement is created since it is a needed
    // parameter to get gpio settings. The actual power choice here does not
    // matter.
    uint32_t dummy_tx_power_cdbm = 0;
    // Enforce usage of the HPF
    enum BasebandFilterType const rx_baseband_filter = BasebandFilterHighpass;

    // Find the appropriate gpio settings
    struct GpioPinsSetClear gpio_pins;
    get_ex10_board_spec()->get_gpio_output_pins_set_clear(
        &gpio_pins,
        antenna,
        dummy_tx_power_cdbm,
        rx_baseband_filter,
        reader.region->get_rf_filter());

    // Set the gpio to enforce the HPF
    reader.ops->set_clear_gpio_pins(&gpio_pins);
    struct OpCompletionStatus op_error = reader.ops->wait_op_completion();
    if (op_error.error_occurred)
    {
        return RSSI_INVALID;
    }

    // Set the override if used
    struct LbtControlFields const lbt_control_settings = {
        .override = override_used,
    };
    get_ex10_protocol()->write(&lbt_control_reg, &lbt_control_settings);
    // Run listen before talk
    reader.ops->run_listen_before_talk(synth_params.r_divider_index,
                                       synth_params.n_divider,
                                       lbt_offset,
                                       rssi_count);

    op_error = reader.ops->wait_op_completion();
    if (op_error.error_occurred)
    {
        return RSSI_INVALID;
    }
    uint16_t const raw_rssi = get_ex10_helpers()->get_rssi_from_fifo_packet();

    struct RxGainControlFields const lbt_rx_gains =
        (override_used) ? *reader.ops->get_current_analog_rx_config()
                        : reader.ops->get_default_lbt_rx_analog_configs();

    uint16_t sku_val    = get_ex10_version()->get_sku();
    int16_t  sku_offset = 0;

    switch (sku_val)
    {
        case SkuE310:
            sku_offset = -2314;
            break;
        default:
            sku_offset = 0;
            break;
    }

    uint16_t curr_temp_adc = 0;
    reader.ops->measure_aux_adc(AdcResultTemperature, 1, &curr_temp_adc);

    return reader.calibration->get_compensated_lbt_rssi(
               raw_rssi,
               &lbt_rx_gains,
               reader.inventory_params.antenna,
               reader.region->get_rf_filter(),
               curr_temp_adc) +
           sku_offset;
}

static struct ContinuousInventoryState volatile const*
    get_continuous_inventory_state(void)
{
    return &(reader.inventory_state);
}

static bool get_return_loss_threshold_exceeded(uint16_t return_loss_cdb,
                                               uint16_t tx_power_cdbm,
                                               uint32_t frequency_khz,
                                               int16_t  max_margin)
{
    // Ensure the function exist in this cal version
    assert(reader.calibration->reverse_power_to_adc);

    // The insertion loss is the loss we expect based on the board. A user
    // should modify this based on the board used. Since the PDET cal table is
    // performed between the antenna port power and the LO pin, the difference
    // in insertion loss betwene antenna and LO pin, vs antenna and RX pin,
    // will need to be considered here to use the LO PDET cal.
    int16_t thresh = tx_power_cdbm - return_loss_cdb -
                     (BOARD_INSERTION_LOSS_RX - BOARD_INSERTION_LOSS_LO) -
                     max_margin;

    // Use the expected threshold found above to find...
    // 1. the reverse power detector to use
    // 2. the corresponding adc target on said reverse power detector
    bool                                temp_comp_enabled = true;
    enum AuxAdcControlChannelEnableBits reverse_power_enables =
        ChannelEnableBitsNone;
    // Note here, the reverse_power_to_adc function sets reverse_power_enables
    // to the appropriate power detector based on the expected threshold.
    // aka we chose a detector in range of our expected readings
    uint16_t reverse_power_adc_threshold =
        reader.calibration->reverse_power_to_adc(thresh,
                                                 frequency_khz,
                                                 reader.last_adc_temperature,
                                                 temp_comp_enabled,
                                                 reader.region->get_rf_filter(),
                                                 &reverse_power_enables);

    enum AuxAdcResultsAdcResult rx_adc_result;
    if (reverse_power_enables == ChannelEnableBitsPowerRx0)
    {
        rx_adc_result = AdcResultPowerRx0;
    }
    else if (reverse_power_enables == ChannelEnableBitsPowerRx1)
    {
        rx_adc_result = AdcResultPowerRx1;
    }
    else if (reverse_power_enables == ChannelEnableBitsPowerRx2)
    {
        rx_adc_result = AdcResultPowerRx2;
    }
    else
    {
        return false;
    }

    uint16_t                  reverse_power_adc = 0u;
    struct OpCompletionStatus op_error =
        reader.ops->measure_aux_adc(rx_adc_result, 1u, &reverse_power_adc);
    if (op_error.error_occurred)
    {
        return false;
    }

    return (reverse_power_adc >= reverse_power_adc_threshold);
}

struct Ex10Reader const* get_ex10_reader(void)
{
    static struct Ex10Reader reader_instance = {
        .init                           = init,
        .init_ex10                      = init_ex10,
        .read_calibration               = read_calibration,
        .deinit                         = deinit,
        .continuous_inventory           = inventory_continuous,
        .inventory                      = inventory,
        .interrupt_handler              = interrupt_handler,
        .fifo_data_handler              = fifo_data_handler,
        .packet_peek                    = packet_peek,
        .packet_remove                  = packet_remove,
        .packets_available              = packets_available,
        .continue_from_halted           = continue_from_halted,
        .cw_test                        = cw_test,
        .write_cal_info_page            = write_cal_info_page,
        .prbs_test                      = prbs_test,
        .ber_test                       = ber_test,
        .etsi_burst_test                = etsi_burst_test,
        .insert_fifo_event              = insert_fifo_event,
        .enable_sdd_logs                = enable_sdd_logs,
        .stop_transmitting              = stop_transmitting,
        .build_cw_configs               = build_cw_configs,
        .get_current_compensated_rssi   = get_current_compensated_rssi,
        .get_current_rssi_log2          = get_current_rssi_log2,
        .get_listen_before_talk_rssi    = get_listen_before_talk_rssi,
        .get_continuous_inventory_state = get_continuous_inventory_state,
        .get_return_loss_threshold_exceeded =
            get_return_loss_threshold_exceeded,
    };

    return &reader_instance;
}
