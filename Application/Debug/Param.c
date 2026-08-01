#include "Application/Debug/Param.h"
#include "Accomplish/26H.h"
#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionLane.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionLineRequirement2.h"
#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/TimedLineRun.h"
#include "Application/Control/Nav.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Stepper.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct
{
    const char *name;
    float (*get)(void);
    void (*set)(float value);
    float minimum;
    float maximum;
} Param_Entry_t;

/* 直接读写模块导出的 Tune 变量；是否立即使用或在 Start 时快照由模块负责。 */
#define PARAM_VAR_ACCESSORS(fn, var)                          \
    static float Param_Get##fn(void) { return (var); }        \
    static void Param_Set##fn(float value) { (var) = value; }

/* 写入变量后还要调用 apply 函数（例如把增益灌进 PID）的参数用这个。 */
#define PARAM_VAR_APPLY_ACCESSORS(fn, var, apply)             \
    static float Param_Get##fn(void) { return (var); }        \
    static void Param_Set##fn(float value)                    \
    {                                                         \
        (var) = value;                                        \
        apply();                                              \
    }

/* K1~K5 是已经发布的双轮批量参数：写入时同时改左右，读取返回两侧均值。
 * 新的左右独立参数追加在表尾，不能重排旧 id。 */
static float Param_GetWheelKp(void)
{
    return (MotionWheel_TuneLeftKp + MotionWheel_TuneRightKp) * 0.5f;
}
static void Param_SetWheelKp(float value)
{
    MotionWheel_TuneLeftKp = value;
    MotionWheel_TuneRightKp = value;
    MotionWheel_ApplyPidTunings();
}
static float Param_GetWheelKi(void)
{
    return (MotionWheel_TuneLeftKi + MotionWheel_TuneRightKi) * 0.5f;
}
static void Param_SetWheelKi(float value)
{
    MotionWheel_TuneLeftKi = value;
    MotionWheel_TuneRightKi = value;
    MotionWheel_ApplyPidTunings();
}
static float Param_GetWheelIntegralLimit(void)
{
    return (MotionWheel_TuneLeftIntegralLimit +
            MotionWheel_TuneRightIntegralLimit) * 0.5f;
}
static void Param_SetWheelIntegralLimit(float value)
{
    MotionWheel_TuneLeftIntegralLimit = value;
    MotionWheel_TuneRightIntegralLimit = value;
    MotionWheel_ApplyPidTunings();
}
static float Param_GetWheelFeedforward(void)
{
    return (MotionWheel_TuneLeftFeedforwardPWMPerMMps +
            MotionWheel_TuneRightFeedforwardPWMPerMMps) * 0.5f;
}
static void Param_SetWheelFeedforward(float value)
{
    MotionWheel_TuneLeftFeedforwardPWMPerMMps = value;
    MotionWheel_TuneRightFeedforwardPWMPerMMps = value;
}
static float Param_GetWheelStaticFriction(void)
{
    return (MotionWheel_TuneLeftStaticFrictionPWM +
            MotionWheel_TuneRightStaticFrictionPWM) * 0.5f;
}
static void Param_SetWheelStaticFriction(float value)
{
    MotionWheel_TuneLeftStaticFrictionPWM = value;
    MotionWheel_TuneRightStaticFrictionPWM = value;
}

PARAM_VAR_APPLY_ACCESSORS(LeftWheelKp, MotionWheel_TuneLeftKp,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_APPLY_ACCESSORS(LeftWheelKi, MotionWheel_TuneLeftKi,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_APPLY_ACCESSORS(LeftWheelIntegralLimit,
                          MotionWheel_TuneLeftIntegralLimit,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_ACCESSORS(LeftWheelFeedforward,
                    MotionWheel_TuneLeftFeedforwardPWMPerMMps)
PARAM_VAR_ACCESSORS(LeftWheelStaticFriction,
                    MotionWheel_TuneLeftStaticFrictionPWM)
PARAM_VAR_APPLY_ACCESSORS(RightWheelKp, MotionWheel_TuneRightKp,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_APPLY_ACCESSORS(RightWheelKi, MotionWheel_TuneRightKi,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_APPLY_ACCESSORS(RightWheelIntegralLimit,
                          MotionWheel_TuneRightIntegralLimit,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_ACCESSORS(RightWheelFeedforward,
                    MotionWheel_TuneRightFeedforwardPWMPerMMps)
PARAM_VAR_ACCESSORS(RightWheelStaticFriction,
                    MotionWheel_TuneRightStaticFrictionPWM)
PARAM_VAR_APPLY_ACCESSORS(StraightKp, MotionStraight_TuneHeadingKp,
                          MotionStraight_ApplyHeadingTunings)
PARAM_VAR_APPLY_ACCESSORS(StraightKd, MotionStraight_TuneHeadingKd,
                          MotionStraight_ApplyHeadingTunings)
PARAM_VAR_ACCESSORS(StraightAcceleration, MotionStraight_TuneAccelerationMMps2)
PARAM_VAR_ACCESSORS(LineKp, MotionLine_TuneKpMMpsPerWeight)
PARAM_VAR_ACCESSORS(LineKi, MotionLine_TuneKiMMpsPerWeight)
PARAM_VAR_ACCESSORS(LineKd, MotionLine_TuneKdMMpsPerWeight)
PARAM_VAR_ACCESSORS(LineCurveKp, MotionLine_TuneKpMMpsPerWeight)
PARAM_VAR_ACCESSORS(LineCurveKd, MotionLine_TuneKdMMpsPerWeight)
static float s_legacyLineCurveSpeedMMps = 20.0f;
static float s_legacyLineCurveHoldDistanceMM = 100.0f;
PARAM_VAR_ACCESSORS(LegacyLineCurveSpeed, s_legacyLineCurveSpeedMMps)
PARAM_VAR_ACCESSORS(LegacyLineCurveHoldDistance,
                    s_legacyLineCurveHoldDistanceMM)
/* K46/K47 已发布，保留为单套巡线 P/D 的兼容别名。 */
PARAM_VAR_ACCESSORS(LaneKp, MotionLane_TuneKp)
PARAM_VAR_ACCESSORS(LaneKdYaw, MotionLane_TuneKdYaw)
PARAM_VAR_ACCESSORS(LaneRatio, MotionLane_TuneMaxAdjustRatio)
PARAM_VAR_ACCESSORS(NavMaxSpeed, Nav_TuneMaxTurnSpeedMMps)
PARAM_VAR_ACCESSORS(NavMinSpeed, Nav_TuneMinTurnSpeedMMps)
PARAM_VAR_ACCESSORS(NavSlowdownAngle, Nav_TuneSlowdownAngleDeg)
PARAM_VAR_ACCESSORS(NavTolerance, Nav_TuneAngleToleranceDeg)
PARAM_VAR_ACCESSORS(CountsPerMM, Odometry_CountsPerMM)
PARAM_VAR_ACCESSORS(H2FinishRollout, Accomplish26H_TuneFinishRolloutMM)
PARAM_VAR_ACCESSORS(H2CruiseSpeed, Accomplish26H_TuneCruiseSpeedMMps)
PARAM_VAR_ACCESSORS(H2FinishSpeed,
                    Accomplish26H_TuneFinishCrawlSpeedMMps)
PARAM_VAR_ACCESSORS(H2StartClear,
                    Accomplish26H_TuneStartClearDistanceMM)
PARAM_VAR_ACCESSORS(H2NominalLap,
                    Accomplish26H_TuneNominalLapDistanceMM)
PARAM_VAR_ACCESSORS(H2FinishApproach,
                    Accomplish26H_TuneFinishApproachDistanceMM)
PARAM_VAR_ACCESSORS(H2MarkerArm,
                    Accomplish26H_TuneFinishMarkerArmDistanceMM)
PARAM_VAR_ACCESSORS(H2MaxLap, Accomplish26H_TuneMaxLapDistanceMM)
PARAM_VAR_ACCESSORS(LineAcceleration, MotionLine_TuneAccelerationMMps2)
PARAM_VAR_ACCESSORS(LineDeceleration, MotionLine_TuneDecelerationMMps2)
PARAM_VAR_ACCESSORS(H2LineKp, MotionLineRequirement2_TuneKpMMpsPerWeight)
PARAM_VAR_ACCESSORS(H2LineKi, MotionLineRequirement2_TuneKiMMpsPerWeight)
PARAM_VAR_ACCESSORS(H2LineKd, MotionLineRequirement2_TuneKdMMpsPerWeight)
PARAM_VAR_ACCESSORS(H2LineAcceleration,
                    MotionLineRequirement2_TuneAccelerationMMps2)
PARAM_VAR_ACCESSORS(H2LineDeceleration,
                    MotionLineRequirement2_TuneDecelerationMMps2)
PARAM_VAR_ACCESSORS(H2RightTurnRightRatio,
                    MotionLineRequirement2_TuneRightTurnRightAdjustRatio)
/* Ball PID and beam calibration. */
PARAM_VAR_ACCESSORS(BallKp, BallBalance_TunePositionKpPerS)
PARAM_VAR_ACCESSORS(BallKi, BallBalance_TuneVelocityKiDegPerMM)
PARAM_VAR_ACCESSORS(BallKd, BallBalance_TuneVelocityKpDegPerMMps)
PARAM_VAR_ACCESSORS(BallMaxVelocity, BallBalance_TuneMaxVelocityMMps)
PARAM_VAR_ACCESSORS(BallFeedforward, BallBalance_TuneFeedforwardDegPerMMps2)
PARAM_VAR_ACCESSORS(BallFeedforwardThreshold, BallBalance_TuneFeedforwardSpeedThresholdMMps)
PARAM_VAR_ACCESSORS(Key2BallKp, BallSequence_TunePositionKpPerS)
PARAM_VAR_ACCESSORS(Key2BallKd, BallSequence_TuneVelocityKpDegPerMMps)
PARAM_VAR_ACCESSORS(Key2BallKi, BallSequence_TuneVelocityKiDegPerMM)
PARAM_VAR_ACCESSORS(Key2PositiveBallKp,
                    BallSequence_TunePositivePositionKpPerS)
PARAM_VAR_ACCESSORS(Key2PositiveBallKd,
                    BallSequence_TunePositiveVelocityKpDegPerMMps)
PARAM_VAR_ACCESSORS(Key2PositiveBallKi,
                    BallSequence_TunePositiveVelocityKiDegPerMM)
PARAM_VAR_ACCESSORS(Key2NegativeMaxVelocity,
                    BallSequence_TuneNegativeMaxVelocityMMps)
PARAM_VAR_ACCESSORS(Key2NegativeApproachDistance,
                    BallSequence_TuneNegativeApproachDistanceMM)
PARAM_VAR_ACCESSORS(Key2NegativeTerminalVelocity,
                    BallSequence_TuneNegativeTerminalVelocityMMps)
PARAM_VAR_ACCESSORS(Key2NegativeMaxTilt,
                    BallSequence_TuneNegativeMaxTiltDeg)
PARAM_VAR_ACCESSORS(Key2NegativeIntegralLimit,
                    BallSequence_TuneNegativeIntegralLimitMM)
PARAM_VAR_ACCESSORS(Key2PositiveMaxVelocity,
                    BallSequence_TunePositiveMaxVelocityMMps)
PARAM_VAR_ACCESSORS(Key2PositiveApproachDistance,
                    BallSequence_TunePositiveApproachDistanceMM)
PARAM_VAR_ACCESSORS(Key2PositiveTerminalVelocity,
                    BallSequence_TunePositiveTerminalVelocityMMps)
PARAM_VAR_ACCESSORS(Key2PositiveMaxTilt,
                    BallSequence_TunePositiveMaxTiltDeg)
PARAM_VAR_ACCESSORS(Key2PositiveIntegralLimit,
                    BallSequence_TunePositiveIntegralLimitMM)
PARAM_VAR_ACCESSORS(BallHalfLength, BallSensor_TuneHalfLengthMM)
PARAM_VAR_ACCESSORS(BeamGearRatio, BeamActuator_TuneGearRatio)
PARAM_VAR_ACCESSORS(BeamZeroOffset, BeamActuator_TuneZeroOffsetDeg)
/* KEY3 定时巡线参数；下一次 KEY3 启动时统一快照。 */
PARAM_VAR_ACCESSORS(Key3Acceleration,
                    TimedLineRun_TuneAccelerationMMps2)
PARAM_VAR_ACCESSORS(Key3CruiseSpeed,
                    TimedLineRun_TuneCruiseSpeedMMps)
PARAM_VAR_ACCESSORS(Key3RunDuration,
                    TimedLineRun_TuneDurationSeconds)
/* 要求 4 保持 O 点：前馈比例现在恒作用在 0 上（车静止），A→B 直线接进来
 * 后才真正起作用，做成可调免去为调前馈强度重新烧录。 */

/* 陀螺仪尺度因子保存在 Heading 内部，经既有接口读写。 */
static float Param_GetGyroScale(void) { return Heading_GetScale(); }
static void Param_SetGyroScale(float value) { Heading_SetScale(value); }

/* 表序即协议 id（下标 0 = K1）。id 一旦发布不得重排，只能在尾部追加。 */
static const Param_Entry_t s_params[] = {
    { "wkp", Param_GetWheelKp, Param_SetWheelKp, 0.0f, 50.0f },
    { "wki", Param_GetWheelKi, Param_SetWheelKi, 0.0f, 50.0f },
    { "wil", Param_GetWheelIntegralLimit, Param_SetWheelIntegralLimit,
      0.0f, 1000.0f },
    { "wff", Param_GetWheelFeedforward, Param_SetWheelFeedforward,
      0.0f, 10.0f },
    { "wsf", Param_GetWheelStaticFriction, Param_SetWheelStaticFriction,
      0.0f, 500.0f },
    { "skp", Param_GetStraightKp, Param_SetStraightKp, 0.0f, 100.0f },
    { "skd", Param_GetStraightKd, Param_SetStraightKd, 0.0f, 50.0f },
    { "sac", Param_GetStraightAcceleration, Param_SetStraightAcceleration,
      10.0f, 5000.0f },
    { "lra", Param_GetLineKp, Param_SetLineKp, 0.0f, 200.0f },
    { "lkd", Param_GetLineKd, Param_SetLineKd, 0.0f, 200.0f },
    { "nvx", Param_GetNavMaxSpeed, Param_SetNavMaxSpeed, 10.0f, 500.0f },
    { "nvn", Param_GetNavMinSpeed, Param_SetNavMinSpeed, 1.0f, 500.0f },
    { "nsa", Param_GetNavSlowdownAngle, Param_SetNavSlowdownAngle,
      5.0f, 180.0f },
    { "ntl", Param_GetNavTolerance, Param_SetNavTolerance, 0.5f, 20.0f },
    { "gsc", Param_GetGyroScale, Param_SetGyroScale, 0.5f, 2.0f },
    { "cpm", Param_GetCountsPerMM, Param_SetCountsPerMM, 0.5f, 50.0f },
    { "lwkp", Param_GetLeftWheelKp, Param_SetLeftWheelKp, 0.0f, 50.0f },
    { "lwki", Param_GetLeftWheelKi, Param_SetLeftWheelKi, 0.0f, 50.0f },
    { "lwil", Param_GetLeftWheelIntegralLimit,
      Param_SetLeftWheelIntegralLimit, 0.0f, 1000.0f },
    { "lwff", Param_GetLeftWheelFeedforward,
      Param_SetLeftWheelFeedforward, 0.0f, 10.0f },
    { "lwsf", Param_GetLeftWheelStaticFriction,
      Param_SetLeftWheelStaticFriction, 0.0f, 500.0f },
    { "rwkp", Param_GetRightWheelKp, Param_SetRightWheelKp, 0.0f, 50.0f },
    { "rwki", Param_GetRightWheelKi, Param_SetRightWheelKi, 0.0f, 50.0f },
    { "rwil", Param_GetRightWheelIntegralLimit,
      Param_SetRightWheelIntegralLimit, 0.0f, 1000.0f },
    { "rwff", Param_GetRightWheelFeedforward,
      Param_SetRightWheelFeedforward, 0.0f, 10.0f },
    { "rwsf", Param_GetRightWheelStaticFriction,
      Param_SetRightWheelStaticFriction, 0.0f, 500.0f },
    /* 视觉巡道三个增益，追加在表尾，保持既有 id 不变。 */
    { "vkp", Param_GetLaneKp, Param_SetLaneKp, 0.0f, 5.0f },
    { "vkd", Param_GetLaneKdYaw, Param_SetLaneKdYaw, 0.0f, 10.0f },
    { "vra", Param_GetLaneRatio, Param_SetLaneRatio, 0.05f, 1.0f },
    { "h2off", Param_GetH2FinishRollout, Param_SetH2FinishRollout,
      0.0f, 300.0f },
    /* 要求 3 摆球标定量，追加在表尾，保持既有 id 不变。 */
    { "bkp", Param_GetBallKp, Param_SetBallKp, 0.0f, 10.0f },
    { "bkd", Param_GetBallKd, Param_SetBallKd, 0.0f, 1.0f },
    { "bki", Param_GetBallKi, Param_SetBallKi, 0.0f, 0.2f },
    { "bhl", Param_GetBallHalfLength, Param_SetBallHalfLength,
      50.0f, 200.0f },
    { "bgr", Param_GetBeamGearRatio, Param_SetBeamGearRatio, 0.1f, 50.0f },
    { "bzo", Param_GetBeamZeroOffset, Param_SetBeamZeroOffset,
      STEPPER_MIN_ANGLE_DEG, STEPPER_MAX_ANGLE_DEG },
    /* 要求 2 仍沿用既有 26H 阶段；这些值只在下一次 KEY1 启动时快照。 */
    { "h2cru", Param_GetH2CruiseSpeed, Param_SetH2CruiseSpeed,
      20.0f, 2000.0f },
    { "h2fin", Param_GetH2FinishSpeed, Param_SetH2FinishSpeed,
      10.0f, 2000.0f },
    { "h2clr", Param_GetH2StartClear, Param_SetH2StartClear,
      0.0f, 1000.0f },
    { "h2lap", Param_GetH2NominalLap, Param_SetH2NominalLap,
      1000.0f, 20000.0f },
    { "h2app", Param_GetH2FinishApproach, Param_SetH2FinishApproach,
      0.0f, 5000.0f },
    { "h2arm", Param_GetH2MarkerArm, Param_SetH2MarkerArm,
      0.0f, 20000.0f },
    { "h2max", Param_GetH2MaxLap, Param_SetH2MaxLap,
      1000.0f, 25000.0f },
    { "lacc", Param_GetLineAcceleration, Param_SetLineAcceleration,
      10.0f, 5000.0f },
    { "ldec", Param_GetLineDeceleration, Param_SetLineDeceleration,
      10.0f, 5000.0f },
    /* 要求 4 保持 O 点，追加在表尾，保持既有 id 不变。增益仍用 bkp/bkd。 */
    /* K46/K47 是已发布的直线 P/D 兼容别名；弧线仅控制低速速度和保持距离。 */
    { "lcra", Param_GetLineCurveKp, Param_SetLineCurveKp,
      0.0f, 200.0f },
    { "lckd", Param_GetLineCurveKd, Param_SetLineCurveKd,
      0.0f, 200.0f },
    { "lcv", Param_GetLegacyLineCurveSpeed,
      Param_SetLegacyLineCurveSpeed,
      20.0f, 1000.0f },
    { "lch", Param_GetLegacyLineCurveHoldDistance,
      Param_SetLegacyLineCurveHoldDistance, 100.0f, 5000.0f },
    { "bvm", Param_GetBallMaxVelocity, Param_SetBallMaxVelocity,
      10.0f, 500.0f },
    { "bff", Param_GetBallFeedforward, Param_SetBallFeedforward,
      0.0f, 0.1f },
    { "bft", Param_GetBallFeedforwardThreshold, Param_SetBallFeedforwardThreshold,
      0.0f, 500.0f },
    /* 巡线现在只有统一 PID，不再暴露弯道低速参数。 */
    { "lki", Param_GetLineKi, Param_SetLineKi, 0.0f, 50.0f },
    /* KEY3 定时巡线：加速度、巡航速度和运行秒数。 */
    { "k3acc", Param_GetKey3Acceleration, Param_SetKey3Acceleration,
      10.0f, 5000.0f },
    { "k3cru", Param_GetKey3CruiseSpeed, Param_SetKey3CruiseSpeed,
      20.0f, 2000.0f },
    { "k3dur", Param_GetKey3RunDuration, Param_SetKey3RunDuration,
      1.0f, 60.0f },
    /* KEY2 phase 1 (O -> -50 mm) gains, kept in their published K57-K59 slots. */
    { "k2kp", Param_GetKey2BallKp, Param_SetKey2BallKp,
      0.0f, 10.0f },
    { "k2kd", Param_GetKey2BallKd, Param_SetKey2BallKd,
      0.0f, 1.0f },
    { "k2ki", Param_GetKey2BallKi, Param_SetKey2BallKi,
      0.0f, 0.2f },
    /* KEY2 phase 2 (-50 mm -> +50 mm) gains are appended as K60-K62. */
    { "k2pkp", Param_GetKey2PositiveBallKp, Param_SetKey2PositiveBallKp,
      0.0f, 10.0f },
    { "k2pkd", Param_GetKey2PositiveBallKd, Param_SetKey2PositiveBallKd,
      0.0f, 1.0f },
    { "k2pki", Param_GetKey2PositiveBallKi, Param_SetKey2PositiveBallKi,
      0.0f, 0.2f },
    /* KEY2 phase motion profiles are appended as K63-K72. */
    { "k2vm", Param_GetKey2NegativeMaxVelocity,
      Param_SetKey2NegativeMaxVelocity, 1.0f, 500.0f },
    { "k2ad", Param_GetKey2NegativeApproachDistance,
      Param_SetKey2NegativeApproachDistance, 0.0f, 200.0f },
    { "k2tv", Param_GetKey2NegativeTerminalVelocity,
      Param_SetKey2NegativeTerminalVelocity, 0.0f, 500.0f },
    { "k2mt", Param_GetKey2NegativeMaxTilt,
      Param_SetKey2NegativeMaxTilt, 0.1f, 30.0f },
    { "k2il", Param_GetKey2NegativeIntegralLimit,
      Param_SetKey2NegativeIntegralLimit, 1.0f, 1000.0f },
    { "k2pvm", Param_GetKey2PositiveMaxVelocity,
      Param_SetKey2PositiveMaxVelocity, 1.0f, 500.0f },
    { "k2pad", Param_GetKey2PositiveApproachDistance,
      Param_SetKey2PositiveApproachDistance, 0.0f, 200.0f },
    { "k2ptv", Param_GetKey2PositiveTerminalVelocity,
      Param_SetKey2PositiveTerminalVelocity, 0.0f, 500.0f },
    { "k2pmt", Param_GetKey2PositiveMaxTilt,
      Param_SetKey2PositiveMaxTilt, 0.1f, 30.0f },
    { "k2pil", Param_GetKey2PositiveIntegralLimit,
      Param_SetKey2PositiveIntegralLimit, 1.0f, 1000.0f },
    /* Requirement 2 line parameters are independent from requirements 4-6. */
    { "h2lkp", Param_GetH2LineKp, Param_SetH2LineKp, 0.0f, 200.0f },
    { "h2lki", Param_GetH2LineKi, Param_SetH2LineKi, 0.0f, 50.0f },
    { "h2lkd", Param_GetH2LineKd, Param_SetH2LineKd, 0.0f, 200.0f },
    { "h2lac", Param_GetH2LineAcceleration,
      Param_SetH2LineAcceleration, 10.0f, 5000.0f },
    { "h2lde", Param_GetH2LineDeceleration,
      Param_SetH2LineDeceleration, 10.0f, 5000.0f },
    { "h2rr", Param_GetH2RightTurnRightRatio,
      Param_SetH2RightTurnRightRatio, 0.1f, 5.0f },
};

#define PARAM_COUNT (sizeof(s_params) / sizeof(s_params[0]))

static void Param_SendList(void)
{
    uint32_t index;

    for (index = 0U; index < PARAM_COUNT; index++)
    {
        const Param_Entry_t *entry = &s_params[index];

        Serial1_Printf("K%lu=%.5f %s [%g,%g]\r\n",
                       (unsigned long)(index + 1U),
                       (double)entry->get(),
                       entry->name,
                       (double)entry->minimum,
                       (double)entry->maximum);
    }
    Serial1_Printf("OK K COUNT=%lu\r\n", (unsigned long)PARAM_COUNT);
}

static void Param_SendValue(uint32_t id)
{
    Serial1_Printf("OK K%lu=%.5f\r\n",
                   (unsigned long)id,
                   (double)s_params[id - 1U].get());
}

void Param_HandleCommand(const char *args)
{
    uint32_t id = 0U;
    const char *cursor = args;
    char *end = NULL;
    float value;

    if ((args == NULL) || (args[0] == '\0'))
    {
        Serial1_SendString("ERR K FORMAT\r\n");
        return;
    }

    if ((args[0] == '?') && (args[1] == '\0'))
    {
        Param_SendList();
        return;
    }

    /* 手写十进制 id 解析：只接受纯数字前缀，避免 strtoul 对 +/- 的宽容。 */
    while ((*cursor >= '0') && (*cursor <= '9'))
    {
        id = id * 10U + (uint32_t)(*cursor - '0');
        if (id > 1000U)
        {
            Serial1_SendString("ERR K ID\r\n");
            return;
        }
        cursor++;
    }
    if (cursor == args)
    {
        Serial1_SendString("ERR K FORMAT\r\n");
        return;
    }
    if ((id == 0U) || (id > PARAM_COUNT))
    {
        Serial1_SendString("ERR K ID\r\n");
        return;
    }

    if ((cursor[0] == '?') && (cursor[1] == '\0'))
    {
        Param_SendValue(id);
        return;
    }

    if (cursor[0] != '=')
    {
        Serial1_SendString("ERR K FORMAT\r\n");
        return;
    }

    value = strtof(cursor + 1, &end);
    if ((end == (cursor + 1)) || (*end != '\0') || (!isfinite(value)))
    {
        Serial1_SendString("ERR K FORMAT\r\n");
        return;
    }
    if ((value < s_params[id - 1U].minimum) ||
        (value > s_params[id - 1U].maximum))
    {
        Serial1_Printf("ERR K RANGE MIN=%g MAX=%g\r\n",
                       (double)s_params[id - 1U].minimum,
                       (double)s_params[id - 1U].maximum);
        return;
    }

    s_params[id - 1U].set(value);
    Param_SendValue(id);
}
