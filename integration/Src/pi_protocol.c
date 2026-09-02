#include "pi_protocol.h"

#include <string.h>

#define PI_SOF_1 0xA5U
#define PI_SOF_2 0x5AU
#define PI_BODY_HEADER_SIZE 6U
#define PI_CRC_SIZE 2U
#define PI_MOTOR_PAYLOAD_SIZE 8U
#define PI_SERVO_PAYLOAD_SIZE 10U
#define PI_DETECTION_PAYLOAD_SIZE 16U

#define PI_CAMERA_SERVO_MIN_US 1150U
#define PI_CAMERA_SERVO_MAX_US 1750U
#define PI_LEFT_ARM_SERVO_MIN_US 1300U
#define PI_LEFT_ARM_SERVO_MAX_US 1900U
#define PI_RIGHT_ARM_SERVO_MIN_US 1100U
#define PI_RIGHT_ARM_SERVO_MAX_US 1900U

static uint16_t ReadU16LE(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t ReadI16LE(const uint8_t *data)
{
    return (int16_t)ReadU16LE(data);
}

static void WriteU16LE(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)(value >> 8);
}

static void WriteI32LE(uint8_t *data, int32_t value)
{
    uint32_t unsigned_value = (uint32_t)value;
    data[0] = (uint8_t)(unsigned_value & 0xFFU);
    data[1] = (uint8_t)((unsigned_value >> 8) & 0xFFU);
    data[2] = (uint8_t)((unsigned_value >> 16) & 0xFFU);
    data[3] = (uint8_t)((unsigned_value >> 24) & 0xFFU);
}

static uint16_t Crc16Ccitt(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;
    uint8_t bit;

    for (index = 0U; index < length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static int16_t ClampMotorCommand(int16_t command)
{
    if (command > 1000)
    {
        return 1000;
    }
    if (command < -1000)
    {
        return -1000;
    }
    return command;
}

static uint16_t ClampServoPulse(
    uint16_t pulse_us,
    uint16_t min_us,
    uint16_t max_us
)
{
    if (pulse_us < min_us)
    {
        return min_us;
    }
    if (pulse_us > max_us)
    {
        return max_us;
    }
    return pulse_us;
}

static void HandleFrame(PiProtocol *protocol)
{
    const uint8_t *body = protocol->frame_buffer;
    uint8_t version = body[0];
    uint8_t message_type = body[1];
    uint16_t payload_length = ReadU16LE(&body[4]);
    const uint8_t *payload = &body[PI_BODY_HEADER_SIZE];
    uint32_t now_ms = HAL_GetTick();

    if (version != PI_PROTOCOL_VERSION)
    {
        return;
    }

    if ((message_type == PI_MSG_HEARTBEAT) && (payload_length == 8U))
    {
        protocol->last_heartbeat_ms = now_ms;
    }
    else if (
        (message_type == PI_MSG_MOTOR_COMMAND) &&
        (payload_length == PI_MOTOR_PAYLOAD_SIZE)
    )
    {
        uint16_t ttl_ms;
        protocol->motor_command.motor_1 =
            ClampMotorCommand(ReadI16LE(&payload[0]));
        protocol->motor_command.motor_2 =
            ClampMotorCommand(ReadI16LE(&payload[2]));
        protocol->motor_command.motor_3 =
            ClampMotorCommand(ReadI16LE(&payload[4]));
        ttl_ms = ReadU16LE(&payload[6]);
        if (ttl_ms < 20U)
        {
            ttl_ms = 20U;
        }
        else if (ttl_ms > 2000U)
        {
            ttl_ms = 2000U;
        }
        protocol->motor_command.ttl_ms = ttl_ms;
        protocol->last_motor_command_ms = now_ms;
        protocol->motor_command_seen = 1U;
        protocol->motor_command_pending = 1U;
    }
    else if (
        (message_type == PI_MSG_SERVO_COMMAND) &&
        (payload_length == PI_SERVO_PAYLOAD_SIZE)
    )
    {
        uint16_t ttl_ms;

            protocol->servo_command.camera_us =
                ClampServoPulse(
                    ReadU16LE(&payload[0]),
                    PI_CAMERA_SERVO_MIN_US,
                    PI_CAMERA_SERVO_MAX_US
                );
            protocol->servo_command.left_arm_us =
                ClampServoPulse(
                    ReadU16LE(&payload[2]),
                    PI_LEFT_ARM_SERVO_MIN_US,
                    PI_LEFT_ARM_SERVO_MAX_US
                );
            protocol->servo_command.right_arm_us =
                ClampServoPulse(
                    ReadU16LE(&payload[4]),
                    PI_RIGHT_ARM_SERVO_MIN_US,
                    PI_RIGHT_ARM_SERVO_MAX_US
                );

        ttl_ms = ReadU16LE(&payload[6]);
        if (ttl_ms < 50U)
        {
            ttl_ms = 50U;
        }
        else if (ttl_ms > 2000U)
        {
            ttl_ms = 2000U;
        }

        protocol->servo_command.ttl_ms = ttl_ms;
        protocol->servo_command.enable_mask = payload[8] & 0x07U;
        protocol->last_servo_command_ms = now_ms;
        protocol->servo_command_seen = 1U;
        protocol->servo_command_pending = 1U;
    }
    else if (
        (message_type == PI_MSG_DETECTION) &&
        (payload_length == PI_DETECTION_PAYLOAD_SIZE)
    )
    {
        protocol->latest_detection.class_id = payload[0];
        protocol->latest_detection.confidence_milli = ReadU16LE(&payload[2]);
        protocol->latest_detection.center_x_milli = ReadI16LE(&payload[4]);
        protocol->latest_detection.center_y_milli = ReadI16LE(&payload[6]);
        protocol->latest_detection.width_milli = ReadU16LE(&payload[8]);
        protocol->latest_detection.height_milli = ReadU16LE(&payload[10]);
        protocol->latest_detection.distance_mm = ReadU16LE(&payload[12]);
        protocol->latest_detection.track_id = ReadU16LE(&payload[14]);
        protocol->latest_detection.received_at_ms = now_ms;
        protocol->detection_valid = 1U;
    }
}

static void ResetParser(PiProtocol *protocol)
{
    protocol->parser_state = 0U;
    protocol->frame_length = 0U;
    protocol->expected_length = 0U;
}

static void PushRxByte(PiProtocol *protocol, uint8_t value)
{
    uint16_t payload_length;
    uint16_t received_crc;
    uint16_t calculated_crc;

    if (protocol->parser_state == 0U)
    {
        if (value == PI_SOF_1)
        {
            protocol->parser_state = 1U;
        }
        return;
    }

    if (protocol->parser_state == 1U)
    {
        if (value == PI_SOF_2)
        {
            protocol->parser_state = 2U;
            protocol->frame_length = 0U;
        }
        else
        {
            protocol->parser_state = (value == PI_SOF_1) ? 1U : 0U;
        }
        return;
    }

    if (protocol->frame_length >= sizeof(protocol->frame_buffer))
    {
        protocol->length_error_count++;
        ResetParser(protocol);
        return;
    }

    protocol->frame_buffer[protocol->frame_length++] = value;
    if (protocol->frame_length == PI_BODY_HEADER_SIZE)
    {
        payload_length = ReadU16LE(&protocol->frame_buffer[4]);
        if (payload_length > PI_PROTOCOL_MAX_PAYLOAD)
        {
            protocol->length_error_count++;
            ResetParser(protocol);
            return;
        }
        protocol->expected_length =
            PI_BODY_HEADER_SIZE + payload_length + PI_CRC_SIZE;
    }

    if (
        (protocol->expected_length != 0U) &&
        (protocol->frame_length == protocol->expected_length)
    )
    {
        received_crc = ReadU16LE(
            &protocol->frame_buffer[protocol->expected_length - PI_CRC_SIZE]
        );
        calculated_crc = Crc16Ccitt(
            protocol->frame_buffer,
            protocol->expected_length - PI_CRC_SIZE
        );
        if (received_crc == calculated_crc)
        {
            HandleFrame(protocol);
        }
        else
        {
            protocol->crc_error_count++;
        }
        ResetParser(protocol);
    }
}

static HAL_StatusTypeDef SendFrame(
    PiProtocol *protocol,
    uint8_t message_type,
    const uint8_t *payload,
    uint16_t payload_length
)
{
    uint8_t frame[2U + PI_BODY_HEADER_SIZE + PI_PROTOCOL_MAX_PAYLOAD + PI_CRC_SIZE];
    uint16_t body_length;
    uint16_t crc;

    if (
        (protocol == NULL) ||
        (protocol->uart == NULL) ||
        (payload_length > PI_PROTOCOL_MAX_PAYLOAD)
    )
    {
        return HAL_ERROR;
    }

    frame[0] = PI_SOF_1;
    frame[1] = PI_SOF_2;
    frame[2] = PI_PROTOCOL_VERSION;
    frame[3] = message_type;
    WriteU16LE(&frame[4], protocol->tx_sequence++);
    WriteU16LE(&frame[6], payload_length);
    if ((payload_length > 0U) && (payload != NULL))
    {
        memcpy(&frame[8], payload, payload_length);
    }
    body_length = PI_BODY_HEADER_SIZE + payload_length;
    crc = Crc16Ccitt(&frame[2], body_length);
    WriteU16LE(&frame[2U + body_length], crc);

    return HAL_UART_Transmit(
        protocol->uart,
        frame,
        (uint16_t)(2U + body_length + PI_CRC_SIZE),
        10U
    );
}

void PiProtocol_Init(PiProtocol *protocol, UART_HandleTypeDef *uart)
{
    if (protocol == NULL)
    {
        return;
    }
    memset(protocol, 0, sizeof(*protocol));
    protocol->uart = uart;
}

HAL_StatusTypeDef PiProtocol_Start(PiProtocol *protocol)
{
    if ((protocol == NULL) || (protocol->uart == NULL))
    {
        return HAL_ERROR;
    }
    return HAL_UART_Receive_IT(protocol->uart, &protocol->rx_byte, 1U);
}

void PiProtocol_OnRxComplete(PiProtocol *protocol, UART_HandleTypeDef *uart)
{
    if (
        (protocol == NULL) ||
        (protocol->uart == NULL) ||
        (uart != protocol->uart)
    )
    {
        return;
    }
    PushRxByte(protocol, protocol->rx_byte);
    (void)HAL_UART_Receive_IT(protocol->uart, &protocol->rx_byte, 1U);
}

void PiProtocol_OnUartError(PiProtocol *protocol, UART_HandleTypeDef *uart)
{
    if (
        (protocol == NULL) ||
        (protocol->uart == NULL) ||
        (uart != protocol->uart)
    )
    {
        return;
    }
    ResetParser(protocol);
    __HAL_UART_CLEAR_OREFLAG(protocol->uart);
    (void)HAL_UART_Receive_IT(protocol->uart, &protocol->rx_byte, 1U);
}

uint8_t PiProtocol_TakeMotorCommand(
    PiProtocol *protocol,
    PiMotorCommand *command
)
{
    uint32_t primask;
    if (
        (protocol == NULL) ||
        (command == NULL) ||
        (protocol->motor_command_pending == 0U)
    )
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *command = protocol->motor_command;
    protocol->motor_command_pending = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
    return 1U;
}

uint8_t PiProtocol_IsMotorCommandExpired(
    const PiProtocol *protocol,
    uint32_t now_ms
)
{
    if ((protocol == NULL) || (protocol->motor_command_seen == 0U))
    {
        return 0U;
    }
    return (
        (uint32_t)(now_ms - protocol->last_motor_command_ms) >
        protocol->motor_command.ttl_ms
    ) ? 1U : 0U;
}

uint8_t PiProtocol_TakeServoCommand(
    PiProtocol *protocol,
    PiServoCommand *command
)
{
    uint32_t primask;
    if (
        (protocol == NULL) ||
        (command == NULL) ||
        (protocol->servo_command_pending == 0U)
    )
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *command = protocol->servo_command;
    protocol->servo_command_pending = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
    return 1U;
}

uint8_t PiProtocol_IsServoCommandExpired(
    const PiProtocol *protocol,
    uint32_t now_ms
)
{
    if ((protocol == NULL) || (protocol->servo_command_seen == 0U))
    {
        return 0U;
    }
    return (
        (uint32_t)(now_ms - protocol->last_servo_command_ms) >
        protocol->servo_command.ttl_ms
    ) ? 1U : 0U;
}

HAL_StatusTypeDef PiProtocol_SendEncoderTelemetry(
    PiProtocol *protocol,
    int32_t encoder_1,
    int32_t encoder_2,
    int32_t encoder_3
)
{
    uint8_t payload[12];
    WriteI32LE(&payload[0], encoder_1);
    WriteI32LE(&payload[4], encoder_2);
    WriteI32LE(&payload[8], encoder_3);
    return SendFrame(
        protocol,
        PI_MSG_ENCODER_TELEMETRY,
        payload,
        sizeof(payload)
    );
}

HAL_StatusTypeDef PiProtocol_SendDistanceTelemetry(
    PiProtocol *protocol,
    uint16_t left_distance_mm,
    uint16_t right_distance_mm,
    uint8_t valid_mask
)
{
    uint8_t payload[6];

    WriteU16LE(&payload[0], left_distance_mm);
    WriteU16LE(&payload[2], right_distance_mm);
    payload[4] = valid_mask;
    payload[5] = 0U;
    return SendFrame(
        protocol,
        PI_MSG_DISTANCE_TELEMETRY,
        payload,
        sizeof(payload)
    );
}
