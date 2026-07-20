#include "Application/Debug/Param.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/Nav.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"
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

/* 直接读写模块导出的运行时变量；写入后无需额外动作的参数用这个。 */
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

PARAM_VAR_APPLY_ACCESSORS(WheelKp, MotionWheel_TuneKp,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_APPLY_ACCESSORS(WheelKi, MotionWheel_TuneKi,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_APPLY_ACCESSORS(WheelIntegralLimit, MotionWheel_TuneIntegralLimit,
                          MotionWheel_ApplyPidTunings)
PARAM_VAR_ACCESSORS(WheelFeedforward, MotionWheel_TuneFeedforwardPWMPerMMps)
PARAM_VAR_ACCESSORS(WheelStaticFriction, MotionWheel_TuneStaticFrictionPWM)
PARAM_VAR_APPLY_ACCESSORS(StraightKp, MotionStraight_TuneHeadingKp,
                          MotionStraight_ApplyHeadingTunings)
PARAM_VAR_APPLY_ACCESSORS(StraightKd, MotionStraight_TuneHeadingKd,
                          MotionStraight_ApplyHeadingTunings)
PARAM_VAR_ACCESSORS(StraightAcceleration, MotionStraight_TuneAccelerationMMps2)
PARAM_VAR_ACCESSORS(LineRatio, MotionLine_TuneMaxAdjustRatio)
PARAM_VAR_ACCESSORS(LineWeightKd, MotionLine_TuneWeightKd)
PARAM_VAR_ACCESSORS(NavMaxSpeed, Nav_TuneMaxTurnSpeedMMps)
PARAM_VAR_ACCESSORS(NavMinSpeed, Nav_TuneMinTurnSpeedMMps)
PARAM_VAR_ACCESSORS(NavSlowdownAngle, Nav_TuneSlowdownAngleDeg)
PARAM_VAR_ACCESSORS(NavTolerance, Nav_TuneAngleToleranceDeg)
PARAM_VAR_ACCESSORS(CountsPerMM, Odometry_CountsPerMM)

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
    { "lra", Param_GetLineRatio, Param_SetLineRatio, 0.01f, 1.0f },
    { "lkd", Param_GetLineWeightKd, Param_SetLineWeightKd, 0.0f, 100.0f },
    { "nvx", Param_GetNavMaxSpeed, Param_SetNavMaxSpeed, 10.0f, 500.0f },
    { "nvn", Param_GetNavMinSpeed, Param_SetNavMinSpeed, 1.0f, 500.0f },
    { "nsa", Param_GetNavSlowdownAngle, Param_SetNavSlowdownAngle,
      5.0f, 180.0f },
    { "ntl", Param_GetNavTolerance, Param_SetNavTolerance, 0.5f, 20.0f },
    { "gsc", Param_GetGyroScale, Param_SetGyroScale, 0.5f, 2.0f },
    { "cpm", Param_GetCountsPerMM, Param_SetCountsPerMM, 0.5f, 50.0f },
};

#define PARAM_COUNT (sizeof(s_params) / sizeof(s_params[0]))

static void Param_SendList(void)
{
    uint32_t index;

    for (index = 0U; index < PARAM_COUNT; index++)
    {
        const Param_Entry_t *entry = &s_params[index];

        Serial1_Printf("K%lu=%.4f %s [%g,%g]\r\n",
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
    Serial1_Printf("OK K%lu=%.4f\r\n",
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
