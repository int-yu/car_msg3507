#include "PID.h"

/* 通用位置式 PID —— 见 PID.h 接口说明 */

static float PID_Clamp(float v, float limit)
{
    if (limit > 0.0f)
    {
        if (v > limit)  { return limit; }
        if (v < -limit) { return -limit; }
    }
    return v;
}

void PID_Init(PID_t *pid, float kp, float ki, float kd, float outMax, float integralMax)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->outMax = outMax;
    pid->integralMax = integralMax;
    pid->integral = 0.0f;
    pid->prevError = 0.0f;
}

void PID_SetTunings(PID_t *pid, float kp, float ki, float kd)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
}

void PID_Reset(PID_t *pid)
{
    pid->integral = 0.0f;
    pid->prevError = 0.0f;
}

float PID_Update(PID_t *pid, float setpoint, float measure, float dt)
{
    float error = setpoint - measure;
    float deriv = 0.0f;
    float out;

    /* 积分(带抗饱和限幅) */
    pid->integral += error * dt;
    pid->integral = PID_Clamp(pid->integral, pid->integralMax);

    /* 微分 */
    if (dt > 0.0f)
    {
        deriv = (error - pid->prevError) / dt;
    }
    pid->prevError = error;

    out = pid->Kp * error + pid->Ki * pid->integral + pid->Kd * deriv;
    return PID_Clamp(out, pid->outMax);
}
