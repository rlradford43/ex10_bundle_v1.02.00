/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2021 Impinj, Inc. All rights reserved.               *
 *                                                                           *
 *****************************************************************************/

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "ex10_api/application_registers.h"
#include "ex10_api/byte_span.h"
#include "ex10_api/ex10_protocol.h"
#include "ex10_api/gen2_tx_command_manager.h"


struct Gen2BufferBuilderVariables
{
    struct TxCommandInfo commands_list[10];
};
static struct Gen2BufferBuilderVariables builder;


static void clear_local_sequence(void)
{
    for (size_t idx = 0u; idx < MaxTxCommandCount; idx++)
    {
        builder.commands_list[idx].valid = false;
    }
}

static struct Gen2TxCommandManagerError clear_command_in_local_sequence(
    uint8_t clear_idx)
{
    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};
    if (clear_idx >= MaxTxCommandCount)
    {
        curr_error.error_occurred = true;
        curr_error.error          = Gen2CommandManagerErrorNumCommands;
        return curr_error;
    }
    builder.commands_list[clear_idx].valid = false;

    curr_error.current_index = clear_idx;
    return curr_error;
}

static void clear_sequence(void)
{
    uint16_t zero_enables[MaxTxCommandCount] = {0u};
    uint16_t enable_bits                     = 0u;

    struct Ex10Protocol const* protocol = get_ex10_protocol();

    // By clearing the lengths, we are telling the device each command is
    // 0 bits long, thus invalidating them.
    protocol->write(&gen2_lengths_reg, zero_enables);

    // Clear the command enables
    struct Gen2AccessEnableFields const access_enable_field = {.access_enables =
                                                                   enable_bits};
    protocol->write(&gen2_access_enable_reg, &access_enable_field);

    struct Gen2SelectEnableFields const select_enable_field = {.select_enables =
                                                                   enable_bits};
    protocol->write(&gen2_select_enable_reg, &select_enable_field);

    struct Gen2AutoAccessEnableFields const auto_access_enable_field = {
        .auto_access_enables = enable_bits};
    get_ex10_protocol()->write(&gen2_auto_access_enable_reg,
                               &auto_access_enable_field);
}

static void buffer_builder_init(void)
{
    for (size_t idx = 0u; idx < MaxTxCommandCount; idx++)
    {
        builder.commands_list[idx].encoded_command.data =
            builder.commands_list[idx].encoded_buffer;
        builder.commands_list[idx].decoded_command.args =
            builder.commands_list[idx].decoded_buffer;
    }
    clear_local_sequence();
}

static struct Gen2TxCommandManagerError write_sequence(void)
{
    uint8_t tx_buffer[gen2_tx_buffer_reg.length];
    memset(&tx_buffer, 0u, sizeof(tx_buffer));
    uint8_t ids_list[MaxTxCommandCount];
    memset(&ids_list, 0u, sizeof(ids_list));
    uint8_t offset_reg_list[MaxTxCommandCount];
    memset(&offset_reg_list, 0u, sizeof(offset_reg_list));
    uint16_t length_reg_list[MaxTxCommandCount];
    memset(&length_reg_list, 0u, sizeof(length_reg_list));

    struct Gen2TxnControlsFields txn_control_list[MaxTxCommandCount];

    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};

    size_t buffer_offset = 0;
    for (size_t idx = 0u; idx < MaxTxCommandCount; idx++)
    {
        if (builder.commands_list[idx].valid)
        {
            // Update the register write for offset, length, and id
            offset_reg_list[idx] = buffer_offset;
            ids_list[idx]        = builder.commands_list[idx].transaction_id;
            // Note this is bit length
            length_reg_list[idx] =
                builder.commands_list[idx].encoded_command.length;
            // find the byte length as well
            size_t byte_size =
                (length_reg_list[idx] - (length_reg_list[idx] % 8)) / 8;
            byte_size += (length_reg_list[idx] % 8) ? 1 : 0;

            // Check the command will fit in the buffer
            if (offset_reg_list[idx] + byte_size > sizeof(tx_buffer))
            {
                curr_error.error_occurred = true;
                curr_error.error          = Gen2CommandManagerErrorBufferLength;
                return curr_error;
            }

            // Update the register write for the gen2 buffer
            memcpy(&tx_buffer[buffer_offset],
                   builder.commands_list[idx].encoded_command.data,
                   byte_size);
            buffer_offset += byte_size;

            // Zero out the struct before setting it
            memset(&txn_control_list[idx],
                   0u,
                   sizeof(struct Gen2TxnControlsFields));
            // Update reg write for tx device controls
            bool control_success =
                get_ex10_gen2_commands()->get_gen2_tx_control_config(
                    &builder.commands_list[idx].decoded_command,
                    &txn_control_list[idx]);
            if (!control_success)
            {
                curr_error.error_occurred = true;
                curr_error.error          = Gen2CommandManagerErrorTxnControls;
                return curr_error;
            }
        }
    }

    // Write all the commands to the buffer
    struct RegisterInfo const* const regs[] = {
        &gen2_offsets_reg,
        &gen2_lengths_reg,
        &gen2_transaction_ids_reg,
        &gen2_txn_controls_reg,
    };

    void const* buffers[] = {
        offset_reg_list,
        length_reg_list,
        ids_list,
        txn_control_list,
    };

    get_ex10_protocol()->write_multiple(
        regs, buffers, sizeof(regs) / sizeof(void const*));

    get_ex10_protocol()->write(&gen2_tx_buffer_reg, tx_buffer);

    return curr_error;
}

static bool get_is_select(enum Gen2Command current_command)
{
    return (current_command == Gen2Select);
}

static struct Gen2TxCommandManagerError write_select_enables(
    bool const* select_enables,
    uint8_t     size)
{
    assert(select_enables);

    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};

    if (size > MaxTxCommandCount)
    {
        curr_error.error_occurred = true;
        curr_error.error          = Gen2CommandManagerErrorNumCommands;
        return curr_error;
    }

    uint16_t select_enable_bits = 0u;
    for (size_t idx = 0u; idx < size; idx++)
    {
        // if there is an interest in enabling this index
        if (select_enables[idx])
        {
            if (builder.commands_list[idx].valid)
            {
                // check if the command being enabled matches this register
                // If not, we will still enable it, but warn the user
                if (!get_is_select(
                        builder.commands_list[idx].decoded_command.command))
                {
                    fprintf(
                        stderr,
                        "NOTE: Enabling a non-select command at index %zd for "
                        "select op.\n",
                        idx);
                    curr_error.error_occurred = true;
                    curr_error.current_index  = idx;
                    curr_error.error =
                        Gen2CommandManagerErrorCommandEnableMismatch;
                }
                select_enable_bits |= select_enables[idx] << idx;
            }
            else
            {
                curr_error.error_occurred = true;
                curr_error.current_index  = idx;
                curr_error.error = Gen2CommandManagerErrorEnabledEmptyCommand;
                return curr_error;
            }
        }
    }

    struct Gen2SelectEnableFields const select_enable_field = {
        .select_enables = select_enable_bits};
    get_ex10_protocol()->write(&gen2_select_enable_reg, &select_enable_field);

    return curr_error;
}

static struct Gen2TxCommandManagerError write_halted_enables(
    bool const* access_enables,
    uint8_t     size)
{
    assert(access_enables);

    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};

    if (size > MaxTxCommandCount)
    {
        curr_error.error_occurred = true;
        curr_error.error          = Gen2CommandManagerErrorNumCommands;
        return curr_error;
    }

    uint16_t access_bits = 0u;
    for (size_t idx = 0u; idx < size; idx++)
    {
        // if there is an interest in enabling this index
        if (access_enables[idx])
        {
            if (builder.commands_list[idx].valid)
            {
                // check if the command being enabled matches this register
                // If not, we will still enable it, but warn the user
                if (get_is_select(
                        builder.commands_list[idx].decoded_command.command))
                {
                    fprintf(stderr,
                            "NOTE: Enabling a select command at index %zd for "
                            "halted.\n",
                            idx);
                    curr_error.error_occurred = true;
                    curr_error.current_index  = idx;
                    curr_error.error =
                        Gen2CommandManagerErrorCommandEnableMismatch;
                }
                access_bits |= access_enables[idx] << idx;
            }
            else
            {
                curr_error.error_occurred = true;
                curr_error.current_index  = idx;
                curr_error.error = Gen2CommandManagerErrorEnabledEmptyCommand;
                return curr_error;
            }
        }
    }

    struct Gen2AccessEnableFields const access_enable_field = {.access_enables =
                                                                   access_bits};
    get_ex10_protocol()->write(&gen2_access_enable_reg, &access_enable_field);

    return curr_error;
}

static struct Gen2TxCommandManagerError write_auto_access_enables(
    bool const* auto_access_enables,
    uint8_t     size)
{
    assert(auto_access_enables);

    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};

    if (size > MaxTxCommandCount)
    {
        curr_error.error_occurred = true;
        curr_error.error          = Gen2CommandManagerErrorNumCommands;
        return curr_error;
    }

    uint16_t auto_access_bits = 0u;
    for (size_t idx = 0u; idx < size; idx++)
    {
        // if there is an interest in enabling this index
        if (auto_access_enables[idx])
        {
            if (builder.commands_list[idx].valid)
            {
                // check if the command being enabled matches this register
                // If not, we will still enable it, but warn the user
                if (get_is_select(
                        builder.commands_list[idx].decoded_command.command))
                {
                    fprintf(
                        stderr,
                        "NOTE: Enabling a select command at index %zd for auto "
                        "access.\n",
                        idx);

                    curr_error.error_occurred = true;
                    curr_error.current_index  = idx;
                    curr_error.error =
                        Gen2CommandManagerErrorCommandEnableMismatch;
                }
                auto_access_bits |= auto_access_enables[idx] << idx;
            }
            else
            {
                curr_error.error_occurred = true;
                curr_error.current_index  = idx;
                curr_error.error = Gen2CommandManagerErrorEnabledEmptyCommand;
                return curr_error;
            }
        }
    }

    struct Gen2AutoAccessEnableFields const auto_access_enable_field = {
        .auto_access_enables = auto_access_bits};
    get_ex10_protocol()->write(&gen2_auto_access_enable_reg,
                               &auto_access_enable_field);

    return curr_error;
}

static struct Gen2TxCommandManagerError append_encoded_command(
    const struct BitSpan* tx_buffer,
    uint8_t               transaction_id)
{
    assert(tx_buffer);

    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};

    // Find the next available slot
    size_t current_index = 0;
    while (builder.commands_list[current_index].valid)
    {
        current_index++;
        // No room, return an error
        if (current_index >= MaxTxCommandCount)
        {
            curr_error.error_occurred = true;
            curr_error.error          = Gen2CommandManagerErrorNumCommands;
            return curr_error;
        }
    }

    // Also store the decoded command for debug and tx configuration registers
    bool decode_success = get_ex10_gen2_commands()->decode_gen2_command(
        &builder.commands_list[current_index].decoded_command, tx_buffer);
    if (!decode_success)
    {
        fprintf(stderr, "Command can not be decoded as the type is invalid\n");
        curr_error.error_occurred = true;
        curr_error.error          = Gen2CommandManagerErrorCommandDecode;
        return curr_error;
    }

    // Store the encoded data
    builder.commands_list[current_index].encoded_command.length =
        tx_buffer->length;
    memcpy(builder.commands_list[current_index].encoded_command.data,
           tx_buffer->data,
           tx_buffer->length);

    builder.commands_list[current_index].valid          = true;
    builder.commands_list[current_index].transaction_id = transaction_id;

    curr_error.current_index = current_index;
    return curr_error;
}

static struct Gen2TxCommandManagerError encode_and_append_command(
    struct Gen2CommandSpec* cmd_spec,
    uint8_t                 transaction_id)
{
    assert(cmd_spec);

    struct Gen2TxCommandManagerError curr_error = {
        .error_occurred = false,
        .error          = Gen2CommandManagerErrorNone,
        .current_index  = 0};

    // Find the next available slot
    size_t current_index = 0;
    while (builder.commands_list[current_index].valid)
    {
        current_index++;
        // No room, return an error
        if (current_index >= MaxTxCommandCount)
        {
            curr_error.error_occurred = true;
            curr_error.error          = Gen2CommandManagerErrorNumCommands;
            return curr_error;
        }
    }

    // Attempt to store the encoded info
    bool encode_success = get_ex10_gen2_commands()->encode_gen2_command(
        cmd_spec, &builder.commands_list[current_index].encoded_command);
    if (!encode_success)
    {
        fprintf(stderr,
                "Command has an invalid command type of %d\n",
                cmd_spec->command);

        curr_error.error_occurred = true;
        curr_error.error          = Gen2CommandManagerErrorCommandEncode;
        return curr_error;
    }

    // Store the decoded info
    builder.commands_list[current_index].decoded_command.command =
        cmd_spec->command;
    memcpy(builder.commands_list[current_index].decoded_command.args,
           cmd_spec->args,
           sizeof(builder.commands_list[current_index].decoded_buffer));

    builder.commands_list[current_index].valid          = true;
    builder.commands_list[current_index].transaction_id = transaction_id;

    curr_error.current_index = current_index;
    return curr_error;
}

static void read_device_to_local_sequence(void)
{
    struct Ex10Protocol const* protocol = get_ex10_protocol();

    struct Gen2OffsetsFields gen2_offsets[MaxTxCommandCount];
    protocol->read(&gen2_offsets_reg, gen2_offsets);

    struct Gen2LengthsFields gen2_lengths[MaxTxCommandCount];
    protocol->read(&gen2_lengths_reg, gen2_lengths);

    uint8_t tx_buffer[gen2_tx_buffer_reg.length];
    protocol->read(&gen2_tx_buffer_reg, tx_buffer);

    for (size_t idx = 0u; idx < MaxTxCommandCount; idx++)
    {
        // 0 length means the command is not valid
        if (gen2_lengths[idx].length != 0)
        {
            // Mark the command as valid
            builder.commands_list[idx].valid = true;
            // Copy the encoded command and length into the encoded storage
            builder.commands_list[idx].encoded_command.length =
                gen2_lengths[idx].length;
            // grab the byte size for copying over the data
            size_t byte_size =
                (gen2_lengths[idx].length - (gen2_lengths[idx].length % 8)) / 8;
            byte_size += (gen2_lengths[idx].length % 8) ? 1 : 0;

            memcpy(builder.commands_list[idx].encoded_command.data,
                   &tx_buffer[gen2_offsets[idx].offset],
                   byte_size);

            // Decode the command from the buffer into the decoded storage
            bool decode_success = get_ex10_gen2_commands()->decode_gen2_command(
                &builder.commands_list[idx].decoded_command,
                &builder.commands_list[idx].encoded_command);

            if (!decode_success)
            {
                fprintf(stderr,
                        "Command number index=%zd, offset=%d, length=%d has an "
                        "invalid command type.\n",
                        idx,
                        gen2_offsets[idx].offset,
                        gen2_lengths[idx].length);
            }
        }
        else
        {
            builder.commands_list[idx].valid = false;
        }
    }
}

static void print_local_sequence(void)
{
    FILE* stream = stdout;

    for (size_t idx = 0u; idx < MaxTxCommandCount; idx++)
    {
        // 0 length means the command is not valid
        if (builder.commands_list[idx].valid)
        {
            fprintf(stream,
                    "Command of length %zd\n",
                    builder.commands_list[idx].encoded_command.length);
            fprintf(stream, "Raw data: ");
            for (size_t buff_idx = 0;
                 buff_idx < builder.commands_list[idx].encoded_command.length;
                 buff_idx++)
            {
                fprintf(
                    stream,
                    "%d, ",
                    builder.commands_list[idx].encoded_command.data[buff_idx]);
            }
            fprintf(stream, "\n");
            // Add your own further debug based on need
            fprintf(stream,
                    "Command type is: %d\n",
                    builder.commands_list[idx].decoded_command.command);
        }
    }
}

static void dump_control_registers(void)
{
    FILE*                      stream   = stdout;
    struct Ex10Protocol const* protocol = get_ex10_protocol();

    const uint32_t tx_buffer_len = gen2_tx_buffer_reg.length;
    uint8_t        gen2_tx_buffer[tx_buffer_len];

    protocol->read(&gen2_tx_buffer_reg, gen2_tx_buffer);
    struct Gen2AccessEnableFields access_enable;
    protocol->read(&gen2_access_enable_reg, &access_enable);
    fprintf(stream, "gen2_tx_buffer: ");
    for (size_t i = 0; i < tx_buffer_len; i++)
    {
        fprintf(stream, "%02x ", gen2_tx_buffer[i]);
    }
    fprintf(stream, "\n");
    const size_t num_entries = gen2_txn_controls_reg.num_entries;

    struct Gen2OffsetsFields gen2_offsets[num_entries];
    protocol->read(&gen2_offsets_reg, gen2_offsets);

    struct Gen2LengthsFields gen2_lengths[num_entries];
    protocol->read(&gen2_lengths_reg, gen2_lengths);

    struct Gen2TxnControlsFields gen2_txn_controls[num_entries];
    protocol->read(&gen2_txn_controls_reg, gen2_txn_controls);

    for (size_t i = 0; i < num_entries; i++)
    {
        fprintf(stream, "gen2_offsets[%zu]: %d\n", i, gen2_offsets[i].offset);
    }

    for (size_t i = 0; i < num_entries; i++)
    {
        fprintf(stream, "gen2_lengths[%zu]: %d\n", i, gen2_lengths[i].length);
    }

    for (size_t i = 0; i < num_entries; i++)
    {
        fprintf(stream, "gen2_txn_controls[%zu]:  ", i);
        fprintf(
            stream, "response_type=%d, ", gen2_txn_controls[i].response_type);
        fprintf(
            stream, "has_header_bit=%d ,", gen2_txn_controls[i].has_header_bit);
        fprintf(
            stream, "use_cover_code=%d, ", gen2_txn_controls[i].use_cover_code);
        fprintf(
            stream, "append_handle=%d, ", gen2_txn_controls[i].append_handle);
        fprintf(stream, "append_crc16=%d, ", gen2_txn_controls[i].append_crc16);
        fprintf(stream,
                "is_kill_command=%d, ",
                gen2_txn_controls[i].is_kill_command);
        fprintf(stream, "rx_length=%d\n", gen2_txn_controls[i].rx_length);
    }
}

static struct TxCommandInfo* get_local_sequence(void)
{
    return builder.commands_list;
}

struct Ex10Gen2TxCommandManager const* get_ex10_gen2_tx_command_manager(void)
{
    static struct Ex10Gen2TxCommandManager g2tcm_instance = {
        .clear_local_sequence            = clear_local_sequence,
        .clear_command_in_local_sequence = clear_command_in_local_sequence,
        .clear_sequence                  = clear_sequence,
        .init                            = buffer_builder_init,
        .write_sequence                  = write_sequence,
        .write_select_enables            = write_select_enables,
        .write_halted_enables            = write_halted_enables,
        .write_auto_access_enables       = write_auto_access_enables,
        .append_encoded_command          = append_encoded_command,
        .encode_and_append_command       = encode_and_append_command,
        .read_device_to_local_sequence   = read_device_to_local_sequence,
        .print_local_sequence            = print_local_sequence,
        .dump_control_registers          = dump_control_registers,
        .get_local_sequence              = get_local_sequence,
    };

    return &g2tcm_instance;
}
