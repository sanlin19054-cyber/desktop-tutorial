#include "motor_control.h"

#include "tim.h"

typedef struct
{
    TIM_HandleTypeDef *timer;
    uint32_t input_1_channel;
    uint32_t input_2_channel;
    int8_t direction_sign;
} MotorOutput;

/*
 * Current bench wiring (physical wheel assignment):
 *   m1 / rear        -> board 2 motor A: PC6/PC7, TIM8 CH1/CH2
 *   m2 / right-front -> board 1 motor B: PC8/PC9, TIM8 CH3/CH4
 *   m3 / left-front  -> board 1 motor A: PE9/PE11, TIM1 CH1/CH2
 *
 * This temporary physical assignment differs from the MotorId names and must
 * be normalized before navigation/kinematics code is enabled.
 *
 * Keep direction_sign at +1 until the wheels are lifted and each wheel's
 * positive direction has been checked. Change only the required entry to -1.
 */
static const MotorOutput motor_outputs[MOTOR_COUNT] = {
    {&htim8, TIM_CHANNEL_1, TIM_CHANNEL_2, 1},
    {&htim8, TIM_CHANNEL_3, TIM_CHANNEL_4, -1},
    {&htim1, TIM_CHANNEL_1, TIM_CHANNEL_2, 1},
};

static uint8_t motor_initialized = 0U;
static uint32_t encoder_last_left_front = 0U;
static uint16_t encoder_last_right_front = 0U;
static uint16_t encoder_last_rear = 0U;
static MotorEncoderDeltas encoder_deltas = {0, 0, 0};

static int16_t ClampCommand(int16_t command)
{
    if (command > MOTOR_CONTROL_COMMAND_MAX)
    {
        return MOTOR_CONTROL_COMMAND_MAX;
    }
    if (command < -MOTOR_CONTROL_COMMAND_MAX)
    {
        return -MOTOR_CONTROL_COMMAND_MAX;
    }
    return command;
}

static void WriteOutput(const MotorOutput *output, int16_t command)
{
    uint32_t brake_pulse;
    uint32_t magnitude;
    uint32_t period;
    uint32_t period_ticks;

    if ((output == NULL) || (output->timer == NULL))
    {
        return;
    }

    command = ClampCommand(command);
    command = (int16_t)(command * output->direction_sign);
    period = __HAL_TIM_GET_AUTORELOAD(output->timer);
    period_ticks = period + 1U;

    /* Remove drive from both inputs before changing direction. */
    __HAL_TIM_SET_COMPARE(
        output->timer,
        output->input_1_channel,
        0U
    );
    __HAL_TIM_SET_COMPARE(
        output->timer,
        output->input_2_channel,
        0U
    );

    if (command == 0)
    {
        return;
    }

    magnitude = (uint32_t)((command > 0) ? command : -command);

    /*
     * AT8236 drive/brake PWM.  The static-high input selects the direction;
     * the other input is high during the braking part of the cycle and low
     * during the driving part.  This preserves the bench-tested +500 state
     * (input 1 at 50% PWM, input 2 high) while making every command from
     * -1000 to +1000 usable.  A zero command keeps both inputs low so the
     * bridge coasts and then enters its low-power state.
     */
    brake_pulse =
        ((MOTOR_CONTROL_COMMAND_MAX - magnitude) * period_ticks) /
        MOTOR_CONTROL_COMMAND_MAX;

    if (command > 0)
    {
        __HAL_TIM_SET_COMPARE(
            output->timer,
            output->input_1_channel,
            brake_pulse
        );
        __HAL_TIM_SET_COMPARE(
            output->timer,
            output->input_2_channel,
            period_ticks
        );
    }
    else
    {
        __HAL_TIM_SET_COMPARE(
            output->timer,
            output->input_1_channel,
            period_ticks
        );
        __HAL_TIM_SET_COMPARE(
            output->timer,
            output->input_2_channel,
            brake_pulse
        );
    }
}

static void StopPwmChannels(void)
{
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_3);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
    (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
}

HAL_StatusTypeDef MotorControl_Init(void)
{
    HAL_StatusTypeDef status;

    motor_initialized = 0U;
    WriteOutput(&motor_outputs[MOTOR_LEFT_FRONT], 0);
    WriteOutput(&motor_outputs[MOTOR_RIGHT_FRONT], 0);
    WriteOutput(&motor_outputs[MOTOR_REAR], 0);

    status = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    if (status == HAL_OK)
        status = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
    if (status == HAL_OK)
        status = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
    if (status == HAL_OK)
        status = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
    if (status == HAL_OK)
        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    if (status == HAL_OK)
        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    if (status != HAL_OK)
    {
        StopPwmChannels();
        return status;
    }

    status = HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    if (status == HAL_OK)
        status = HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    if (status == HAL_OK)
        status = HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    if (status != HAL_OK)
    {
        WriteOutput(&motor_outputs[MOTOR_LEFT_FRONT], 0);
        WriteOutput(&motor_outputs[MOTOR_RIGHT_FRONT], 0);
        WriteOutput(&motor_outputs[MOTOR_REAR], 0);
        StopPwmChannels();
        return status;
    }

    encoder_last_left_front = __HAL_TIM_GET_COUNTER(&htim2);
    encoder_last_right_front = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    encoder_last_rear = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    encoder_deltas.left_front = 0;
    encoder_deltas.right_front = 0;
    encoder_deltas.rear = 0;
    motor_initialized = 1U;
    return HAL_OK;
}

void MotorControl_Set(MotorId motor, int16_t command)
{
    if ((motor_initialized == 0U) || ((uint32_t)motor >= MOTOR_COUNT))
    {
        return;
    }
    WriteOutput(&motor_outputs[motor], command);
}

void MotorControl_SetAll(
    int16_t left_front,
    int16_t right_front,
    int16_t rear
)
{
    MotorControl_Set(MOTOR_LEFT_FRONT, left_front);
    MotorControl_Set(MOTOR_RIGHT_FRONT, right_front);
    MotorControl_Set(MOTOR_REAR, rear);
}

void MotorControl_StopAll(void)
{
    if (motor_initialized == 0U)
    {
        return;
    }
    WriteOutput(&motor_outputs[MOTOR_LEFT_FRONT], 0);
    WriteOutput(&motor_outputs[MOTOR_RIGHT_FRONT], 0);
    WriteOutput(&motor_outputs[MOTOR_REAR], 0);
}

void MotorControl_SampleEncoders(void)
{
    uint32_t now_left_front;
    uint16_t now_right_front;
    uint16_t now_rear;

    if (motor_initialized == 0U)
    {
        return;
    }

    now_left_front = __HAL_TIM_GET_COUNTER(&htim2);
    now_right_front = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    now_rear = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);

    encoder_deltas.left_front =
        (int32_t)(now_left_front - encoder_last_left_front);
    encoder_deltas.right_front =
        (int16_t)(now_right_front - encoder_last_right_front);
    encoder_deltas.rear = (int16_t)(now_rear - encoder_last_rear);

    encoder_last_left_front = now_left_front;
    encoder_last_right_front = now_right_front;
    encoder_last_rear = now_rear;
}

MotorEncoderDeltas MotorControl_GetEncoderDeltas(void)
{
    return encoder_deltas;
}

uint8_t MotorControl_IsInitialized(void)
{
    return motor_initialized;
}
