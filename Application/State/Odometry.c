#include "Odometry.h"
#include "Hardware/Motor/Encoder.h"
#include "System/Tick.h"

/* 初始参考值，必须通过实车定距滚动测试进一步标定。 */
float Odometry_CountsPerMM = 6.23f;

static float s_distanceL = 0.0f;
static float s_distanceR = 0.0f;
static float s_speedL = 0.0f;
static float s_speedR = 0.0f;

void Odometry_Init(void)
{
    Encoder_Init();
    Odometry_Reset();
}

void Odometry_Update(uint8_t ticks)
{
    int16_t deltaL = Encoder_Get(1);
    int16_t deltaR = Encoder_Get(2);

    if (ticks < 1U)
    {
        ticks = 1U;
    }

    if (Odometry_CountsPerMM > 0.001f)
    {
        s_distanceL += (float)deltaL / Odometry_CountsPerMM;
        s_distanceR += (float)deltaR / Odometry_CountsPerMM;
        s_speedL = ((float)deltaL / Odometry_CountsPerMM) /
                   ((float)ticks * TICK_DT);
        s_speedR = ((float)deltaR / Odometry_CountsPerMM) /
                   ((float)ticks * TICK_DT);
    }
    else
    {
        s_speedL = 0.0f;
        s_speedR = 0.0f;
    }
}

void Odometry_Reset(void)
{
    (void)Encoder_Get(1);
    (void)Encoder_Get(2);
    s_distanceL = 0.0f;
    s_distanceR = 0.0f;
    s_speedL = 0.0f;
    s_speedR = 0.0f;
}

float Odometry_GetDistanceMM(void)
{
    return (s_distanceL + s_distanceR) * 0.5f;
}

float Odometry_GetDistanceLMM(void)
{
    return s_distanceL;
}

float Odometry_GetDistanceRMM(void)
{
    return s_distanceR;
}

float Odometry_GetSpeedL(void)
{
    return s_speedL;
}

float Odometry_GetSpeedR(void)
{
    return s_speedR;
}
