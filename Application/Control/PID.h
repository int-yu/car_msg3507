#ifndef APPLICATION_CONTROL_PID_H
#define APPLICATION_CONTROL_PID_H

typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    float integral;
    float prevError;
    float outMax;
    float integralMax;
} PID_t;

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float outMax, float integralMax);
void PID_SetTunings(PID_t *pid, float kp, float ki, float kd);
void PID_Reset(PID_t *pid);
float PID_Update(PID_t *pid, float setpoint, float measure, float dt);

#endif
