#ifndef HARDWARE_MOTOR_STEPPER_H
#define HARDWARE_MOTOR_STEPPER_H

#include <stdbool.h>
#include <stdint.h>

#define STEPPER_STEPS_PER_REVOLUTION          3200U
#define STEPPER_ENCODER_COUNTS_PER_REVOLUTION 4096U
#define STEPPER_MIN_STEP_RATE_HZ              64U
#define STEPPER_MAX_STEP_RATE_HZ              16000U

typedef enum {
    STEPPER_RESULT_OK = 0,
    STEPPER_RESULT_INVALID_ARGUMENT,
    STEPPER_RESULT_BUSY,
    STEPPER_RESULT_DISABLED,
    STEPPER_RESULT_NOT_READY
} Stepper_Result_t;

typedef enum {
    STEPPER_STATE_DISABLED = 0,
    STEPPER_STATE_READY,
    STEPPER_STATE_MOVING,
    STEPPER_STATE_STOPPING,
    STEPPER_STATE_FAULT
} Stepper_State_t;

typedef struct {
    uint32_t startStepRateHz;
    uint32_t maxStepRateHz;
    uint32_t accelerationStepsPerSec2;
} Stepper_Profile_t;

typedef struct {
    bool enabled;
    bool ready;
    bool busy;
    bool pwmValid;
    int32_t targetSteps;
    int32_t emittedSteps;
    int32_t encoderCounts;
    int32_t trackingErrorCounts;
    uint16_t absoluteCode;
    float absoluteAngleDeg;
    float multiTurnAngleDeg;
    uint32_t encoderTransitionErrors;
    Stepper_State_t state;
} Stepper_Status_t;

void Stepper_Init(void);
void Stepper_Update(uint8_t elapsedTicks);
Stepper_Result_t Stepper_Enable(bool enable);
Stepper_Result_t Stepper_MoveBySteps(
    int32_t steps, const Stepper_Profile_t *profile);
Stepper_Result_t Stepper_MoveToSteps(
    int32_t target, const Stepper_Profile_t *profile);
Stepper_Result_t Stepper_MoveByAngle(
    float degrees, const Stepper_Profile_t *profile);
Stepper_Result_t Stepper_MoveToAngle(
    float degrees, const Stepper_Profile_t *profile);
void Stepper_Stop(void);
void Stepper_EmergencyStop(void);
Stepper_Result_t Stepper_SetCurrentPosition(float degrees);
bool Stepper_IsBusy(void);
void Stepper_GetStatus(Stepper_Status_t *status);

#endif
