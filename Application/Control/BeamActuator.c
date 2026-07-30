#include "Application/Control/BeamActuator.h"
#include "Hardware/Motor/Stepper.h"
#include <math.h>

float BeamActuator_TuneGearRatio = BEAM_ACTUATOR_GEAR_RATIO;
float BeamActuator_TuneZeroOffsetDeg = 0.0f;

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

static float BeamActuator_Clamp(float value, float limit)
{
    if (value > limit)
    {
        return limit;
    }
    if (value < -limit)
    {
        return -limit;
    }
    return value;
}

void BeamActuator_Init(void)
{
    s_context.requestedTiltDeg = 0.0f;
    s_context.appliedTiltDeg = 0.0f;
    BeamActuator_TuneGearRatio = BEAM_ACTUATOR_GEAR_RATIO;
    BeamActuator_TuneZeroOffsetDeg = 0.0f;

    /* Stepper_Init() 由 App_Init() 统一调用，这里不重复初始化硬件，
     * 只接管它：使能驱动并把上电位置声明为水平零点。 */
    (void)Stepper_Enable(true);
    (void)Stepper_SetCurrentPosition(0.0f);
}

void BeamActuator_SetTiltDeg(float tiltDeg)
{
    if (!isfinite(tiltDeg))
    {
        return;
    }
    s_context.requestedTiltDeg =
        BeamActuator_Clamp(tiltDeg, BEAM_ACTUATOR_MAX_TILT_DEG);
}

void BeamActuator_Update(float dt)
{
    float maximumStepDeg;
    float errorDeg;
    float motorAngleDeg;

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
    s_context.appliedTiltDeg += errorDeg;

    /* 摆杆倾角 -> 电机轴角度。传动比与零点偏置只在这里出现。 */
    motorAngleDeg =
        (s_context.appliedTiltDeg + BeamActuator_TuneZeroOffsetDeg) *
        BeamActuator_TuneGearRatio;
    (void)Stepper_TrackToAngle(motorAngleDeg, &s_beamProfile);
}

float BeamActuator_GetTiltDeg(void)
{
    return s_context.appliedTiltDeg;
}

float BeamActuator_GetRequestedTiltDeg(void)
{
    return s_context.requestedTiltDeg;
}
