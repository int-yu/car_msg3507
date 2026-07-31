#include "Application/Control/BeamActuator.h"
#include "Hardware/Motor/Stepper.h"
#include <math.h>

float BeamActuator_TuneGearRatio = BEAM_ACTUATOR_GEAR_RATIO;
float BeamActuator_TuneZeroOffsetDeg = STEPPER_INITIAL_ANGLE_DEG;

typedef struct
{
    float requestedTiltDeg;
    float appliedTiltDeg;
} BeamActuator_Context_t;

static BeamActuator_Context_t s_context;

/* 上电位姿即水平零点：要求 3 规定钢球起始在中心点 O，摆杆本来就该
 * 水平。没有绝对编码器时这是唯一可用的基准，偏差由 TuneZeroOffsetDeg
 * 手工校正。零点误差是内环扰动，会被外层钢球 PD 吸收：差一步
 * (0.1125 度) 只造成约 0.5 mm 的稳态球位偏移。 */
static const Stepper_Profile_t s_beamProfile = {
    .startStepRateHz = 200U,
    .maxStepRateHz = 4000U,
    .accelerationStepsPerSec2 = 20000U
};

void BeamActuator_Init(void)
{
    s_context.requestedTiltDeg = 0.0f;
    s_context.appliedTiltDeg = 0.0f;
    BeamActuator_TuneGearRatio = BEAM_ACTUATOR_GEAR_RATIO;
    BeamActuator_TuneZeroOffsetDeg = STEPPER_INITIAL_ANGLE_DEG;

    /*
     * Stepper_Init() acquires the MT6816 absolute reference and moves to the
     * configured horizontal angle. Do not overwrite that absolute coordinate.
     */
    (void)Stepper_Enable(true);
}

void BeamActuator_SetTiltDeg(float tiltDeg)
{
    if (!isfinite(tiltDeg))
    {
        return;
    }
    s_context.requestedTiltDeg = tiltDeg;
}

void BeamActuator_Update(float dt)
{
    float maximumStepDeg;
    float errorDeg;
    float nextAppliedTiltDeg;
    float motorAngleDeg;
    Stepper_Result_t result;

    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        return;
    }

    /* 限斜率只为防丢步；设太小会让外环爬不过回差和静摩擦死区。 */
    maximumStepDeg = BEAM_ACTUATOR_MAX_RATE_DEG_PER_S * dt;
    errorDeg = s_context.requestedTiltDeg - s_context.appliedTiltDeg;
    if (errorDeg > maximumStepDeg)
    {
        errorDeg = maximumStepDeg;
    }
    else if (errorDeg < -maximumStepDeg)
    {
        errorDeg = -maximumStepDeg;
    }
    nextAppliedTiltDeg = s_context.appliedTiltDeg + errorDeg;

    /* 摆杆倾角 -> 电机轴角度。传动比与零点偏置只在这里出现。 */
    motorAngleDeg =
        BeamActuator_TuneZeroOffsetDeg +
        BEAM_ACTUATOR_STEPPER_DIRECTION_SIGN *
        nextAppliedTiltDeg * BeamActuator_TuneGearRatio;
    result = Stepper_TrackToAngle(motorAngleDeg, &s_beamProfile);
    if ((result == STEPPER_RESULT_OK) || (result == STEPPER_RESULT_LIMIT))
    {
        s_context.appliedTiltDeg = nextAppliedTiltDeg;
    }
}

float BeamActuator_GetTiltDeg(void)
{
    return s_context.appliedTiltDeg;
}

float BeamActuator_GetRequestedTiltDeg(void)
{
    return s_context.requestedTiltDeg;
}
