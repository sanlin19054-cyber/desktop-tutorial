#include "stp23l.h"

#include <string.h>

#define STP23L_HEADER_SIZE          10U
#define STP23L_SAMPLE_SIZE          15U
#define STP23L_SAMPLE_DATA_OFFSET   10U
#define STP23L_CONFIDENCE_OFFSET     8U
#define STP23L_MIN_DISTANCE_MM      30U
#define STP23L_MAX_DISTANCE_MM    7500U
#define STP23L_MIN_CONFIDENCE       50U
#define STP23L_MIN_VALID_SAMPLES     6U

static uint16_t ReadU16LE(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint8_t HeaderIsValid(const uint8_t *frame)
{
    return (
        (frame[0] == 0xAAU) &&
        (frame[1] == 0xAAU) &&
        (frame[2] == 0xAAU) &&
        (frame[3] == 0xAAU) &&
        (frame[4] == 0x00U) &&
        (frame[5] == 0x02U) &&
        (frame[6] == 0x00U) &&
        (frame[7] == 0x00U) &&
        (frame[8] == 0xB8U) &&
        (frame[9] == 0x00U)
    ) ? 1U : 0U;
}

static void ResetFrameParser(Stp23l *sensor)
{
    sensor->frame_position = 0U;
}

static void PreserveTrailingSyncBytes(Stp23l *sensor)
{
    uint16_t index = sensor->frame_position;
    uint16_t aa_count = 0U;

    while (
        (index > 0U) &&
        (aa_count < 4U) &&
        (sensor->frame_buffer[index - 1U] == 0xAAU)
    )
    {
        --index;
        ++aa_count;
    }

    memset(sensor->frame_buffer, 0, sizeof(sensor->frame_buffer));
    for (index = 0U; index < aa_count; ++index)
    {
        sensor->frame_buffer[index] = 0xAAU;
    }
    sensor->frame_position = aa_count;
}

static void SortU16(uint16_t *values, uint8_t count)
{
    uint8_t index;

    for (index = 1U; index < count; ++index)
    {
        uint16_t value = values[index];
        uint8_t position = index;

        while ((position > 0U) && (values[position - 1U] > value))
        {
            values[position] = values[position - 1U];
            --position;
        }
        values[position] = value;
    }
}

static void HandleFrame(Stp23l *sensor)
{
    uint16_t valid_distances[STP23L_SAMPLE_COUNT];
    uint16_t confidence_sum = 0U;
    uint8_t valid_count = 0U;
    uint8_t sample_index;

    /*
     * The observed STP-23L stream is a fixed 195-byte frame. The vendor's
     * checksum definition is still pending, so this stage validates the
     * fixed header plus at least 6 plausible, high-confidence samples. Add
     * the official checksum here before using this sensor as the sole safety
     * mechanism.
     */
    if (HeaderIsValid(sensor->frame_buffer) == 0U)
    {
        sensor->invalid_frame_count++;
        return;
    }

    sensor->frame_count++;
    for (sample_index = 0U; sample_index < STP23L_SAMPLE_COUNT; ++sample_index)
    {
        uint16_t offset = (uint16_t)(
            STP23L_SAMPLE_DATA_OFFSET +
            ((uint16_t)sample_index * STP23L_SAMPLE_SIZE)
        );
        uint16_t distance = ReadU16LE(&sensor->frame_buffer[offset]);
        uint8_t confidence = sensor->frame_buffer[
            offset + STP23L_CONFIDENCE_OFFSET
        ];

        if (
            (distance >= STP23L_MIN_DISTANCE_MM) &&
            (distance <= STP23L_MAX_DISTANCE_MM) &&
            (confidence >= STP23L_MIN_CONFIDENCE)
        )
        {
            valid_distances[valid_count++] = distance;
            confidence_sum = (uint16_t)(confidence_sum + confidence);
        }
    }

    if (valid_count < STP23L_MIN_VALID_SAMPLES)
    {
        sensor->invalid_frame_count++;
        return;
    }

    SortU16(valid_distances, valid_count);
    if ((valid_count & 1U) != 0U)
    {
        sensor->distance_mm = valid_distances[valid_count / 2U];
    }
    else
    {
        sensor->distance_mm = (uint16_t)(
            ((uint32_t)valid_distances[(valid_count / 2U) - 1U] +
             (uint32_t)valid_distances[valid_count / 2U]) / 2U
        );
    }
    sensor->confidence = (uint8_t)(confidence_sum / valid_count);
    sensor->last_update_ms = HAL_GetTick();
    sensor->valid = 1U;
}

static void PushByte(Stp23l *sensor, uint8_t value)
{
    if (sensor->frame_position < 4U)
    {
        if (value == 0xAAU)
        {
            sensor->frame_buffer[sensor->frame_position++] = value;
        }
        else
        {
            ResetFrameParser(sensor);
        }
        return;
    }

    sensor->frame_buffer[sensor->frame_position++] = value;

    if (
        (sensor->frame_position == STP23L_HEADER_SIZE) &&
        (HeaderIsValid(sensor->frame_buffer) == 0U)
    )
    {
        PreserveTrailingSyncBytes(sensor);
        return;
    }

    if (sensor->frame_position == STP23L_FRAME_SIZE)
    {
        HandleFrame(sensor);
        ResetFrameParser(sensor);
    }
}

static void FeedDmaRange(Stp23l *sensor, uint16_t start, uint16_t end)
{
    uint16_t index;

    for (index = start; index < end; ++index)
    {
        PushByte(sensor, sensor->dma_buffer[index]);
    }
}

void Stp23l_Init(Stp23l *sensor, UART_HandleTypeDef *uart)
{
    if (sensor == NULL)
    {
        return;
    }

    memset(sensor, 0, sizeof(*sensor));
    sensor->uart = uart;
    sensor->distance_mm = STP23L_INVALID_DISTANCE_MM;
}

HAL_StatusTypeDef Stp23l_Start(Stp23l *sensor)
{
    HAL_StatusTypeDef status;

    if (
        (sensor == NULL) ||
        (sensor->uart == NULL) ||
        (sensor->uart->hdmarx == NULL)
    )
    {
        return HAL_ERROR;
    }

    ResetFrameParser(sensor);
    status = HAL_UART_Receive_DMA(
        sensor->uart,
        sensor->dma_buffer,
        STP23L_DMA_BUFFER_SIZE
    );
    sensor->restart_requested = (status == HAL_OK) ? 0U : 1U;
    return status;
}

void Stp23l_Service(Stp23l *sensor)
{
    if ((sensor == NULL) || (sensor->restart_requested == 0U))
    {
        return;
    }

    (void)HAL_UART_AbortReceive(sensor->uart);
    __HAL_UART_CLEAR_PEFLAG(sensor->uart);
    __HAL_UART_CLEAR_FEFLAG(sensor->uart);
    __HAL_UART_CLEAR_NEFLAG(sensor->uart);
    __HAL_UART_CLEAR_OREFLAG(sensor->uart);
    (void)Stp23l_Start(sensor);
}

void Stp23l_OnRxHalfComplete(Stp23l *sensor, UART_HandleTypeDef *uart)
{
    if ((sensor == NULL) || (uart != sensor->uart))
    {
        return;
    }

    FeedDmaRange(sensor, 0U, STP23L_DMA_BUFFER_SIZE / 2U);
}

void Stp23l_OnRxComplete(Stp23l *sensor, UART_HandleTypeDef *uart)
{
    if ((sensor == NULL) || (uart != sensor->uart))
    {
        return;
    }

    FeedDmaRange(
        sensor,
        STP23L_DMA_BUFFER_SIZE / 2U,
        STP23L_DMA_BUFFER_SIZE
    );
}

void Stp23l_OnUartError(Stp23l *sensor, UART_HandleTypeDef *uart)
{
    if ((sensor == NULL) || (uart != sensor->uart))
    {
        return;
    }

    sensor->uart_error_count++;
    /*
     * Keep the last good measurement while the DMA receiver is restarted.
     * Stp23l_GetDistance() still invalidates it when maximum_age_ms is
     * exceeded, so a persistent link failure cannot leave stale data valid.
     */
    sensor->restart_requested = 1U;
}

uint8_t Stp23l_GetDistance(
    const Stp23l *sensor,
    uint32_t now_ms,
    uint32_t maximum_age_ms,
    uint16_t *distance_mm
)
{
    if (
        (sensor == NULL) ||
        (distance_mm == NULL) ||
        (sensor->valid == 0U) ||
        ((uint32_t)(now_ms - sensor->last_update_ms) > maximum_age_ms)
    )
    {
        if (distance_mm != NULL)
        {
            *distance_mm = STP23L_INVALID_DISTANCE_MM;
        }
        return 0U;
    }

    *distance_mm = sensor->distance_mm;
    return 1U;
}
