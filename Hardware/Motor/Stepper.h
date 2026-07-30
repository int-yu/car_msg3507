#ifndef HARDWARE_MOTOR_STEPPER_H
#define HARDWARE_MOTOR_STEPPER_H

#include <stdbool.h>
#include <stdint.h>

/* Mechanical and feedback scale for MS42CG with 16 microsteps. */
#define STEPPER_STEPS_PER_REVOLUTION          3200U
#define STEPPER_ENCODER_COUNTS_PER_REVOLUTION 4096U
#define STEPPER_MIN_STEP_RATE_HZ              64U
#define STEPPER_MAX_STEP_RATE_HZ              16000U
#define STEPPER_UPDATE_PERIOD_MS              10U

/*
 * Hardware-specific startup and travel configuration.
 * Change these values when the mechanism or encoder mounting changes.
 * Coordinate convention: increasing angle moves upward; decreasing angle
 * moves downward.
 */
#ifndef STEPPER_AUTO_START_ENABLED
#define STEPPER_AUTO_START_ENABLED             1U
#endif
#ifndef STEPPER_INITIAL_ANGLE_DEG
#define STEPPER_INITIAL_ANGLE_DEG              139.7f
#endif
#ifndef STEPPER_MIN_ANGLE_DEG
#define STEPPER_MIN_ANGLE_DEG                  80.9f
#endif
#ifndef STEPPER_MAX_ANGLE_DEG
#define STEPPER_MAX_ANGLE_DEG                  210.0f
#endif
#ifndef STEPPER_STARTUP_START_RATE_HZ
#define STEPPER_STARTUP_START_RATE_HZ          200U
#endif
#ifndef STEPPER_STARTUP_MAX_RATE_HZ
#define STEPPER_STARTUP_MAX_RATE_HZ            3200U
#endif
#ifndef STEPPER_STARTUP_ACCELERATION_STEPS_S2
#define STEPPER_STARTUP_ACCELERATION_STEPS_S2  6400U
#endif

typedef enum {
    STEPPER_RESULT_OK = 0,
    STEPPER_RESULT_INVALID_ARGUMENT,
    STEPPER_RESULT_BUSY,
    STEPPER_RESULT_DISABLED,
    STEPPER_RESULT_NOT_READY,
    STEPPER_RESULT_LIMIT
} Stepper_Result_t;

typedef enum {
    STEPPER_STATE_DISABLED = 0,
    STEPPER_STATE_READY,
    STEPPER_STATE_MOVING,
    STEPPER_STATE_STOPPING,
    STEPPER_STATE_FAULT
} Stepper_State_t;

/*
 * Rates are in ST rising edges per second. Acceleration is in rising edges
 * per second squared. The start and maximum rates must remain within the
 * STEPPER_MIN/MAX_STEP_RATE_HZ limits.
 */
typedef struct {
    uint32_t startStepRateHz;
    uint32_t maxStepRateHz;
    uint32_t accelerationStepsPerSec2;
} Stepper_Profile_t;

typedef struct {
    bool enabled;                    /* EN output is active high. */
    bool ready;                      /* Startup absolute reference acquired. */
    bool busy;                       /* MOVING or STOPPING. */
    bool pwmValid;                   /* A recent MT6816 PWM frame is valid. */
    int32_t targetSteps;             /* Absolute software target in ST edges. */
    int32_t emittedSteps;            /* Accounted absolute ST position. */
    int32_t encoderCounts;           /* AB multi-turn position, 4096/rev. */
    int32_t trackingErrorCounts;     /* AB actual minus ST expected. */
    uint16_t absoluteCode;           /* Last PWM code, 0..4095. */
    float absoluteAngleDeg;          /* Last single-turn PWM angle. */
    float multiTurnAngleDeg;         /* AB position converted to degrees. */
    uint32_t encoderTransitionErrors;/* Illegal two-bit AB transitions. */
    Stepper_State_t state;
} Stepper_Status_t;

/*
 * Initializes the AB decoder, capture state and ST timer control. SysConfig
 * power, pin mux and timer initialization must be complete before this call.
 * With auto-start enabled, EN is asserted after safe timer setup. Three valid
 * PWM frames establish the absolute reference, then motion starts toward
 * STEPPER_INITIAL_ANGLE_DEG.
 */
void Stepper_Init(void);

/*
 * Services PWM validation and timeout accounting. One elapsed tick is 10 ms;
 * call from the 100 Hz main loop and pass the actual accumulated tick count.
 */
void Stepper_Update(uint8_t elapsedTicks);

/* Enables or disables the driver. Disabling performs an immediate stop. */
Stepper_Result_t Stepper_Enable(bool enable);

/* Starts a relative move. Targets outside the configured limits are rejected. */
Stepper_Result_t Stepper_MoveBySteps(
    int32_t steps, const Stepper_Profile_t *profile);

/* Starts a limited move to an absolute software step coordinate. */
Stepper_Result_t Stepper_MoveToSteps(
    int32_t target, const Stepper_Profile_t *profile);

/* Starts a limited relative move; degrees are rounded to whole ST edges. */
Stepper_Result_t Stepper_MoveByAngle(
    float degrees, const Stepper_Profile_t *profile);

/* Starts a move to a limited absolute software angle coordinate. */
Stepper_Result_t Stepper_MoveToAngle(
    float degrees, const Stepper_Profile_t *profile);

/* Requests trapezoidal deceleration to a stop. EN remains active. */
void Stepper_Stop(void);

/* Stops ST immediately. EN remains active unless Stepper_Enable(false) is used. */
void Stepper_EmergencyStop(void);

/* Reassigns the limited current idle coordinate without moving the motor. */
Stepper_Result_t Stepper_SetCurrentPosition(float degrees);

bool Stepper_IsBusy(void);
void Stepper_GetStatus(Stepper_Status_t *status);

#endif
