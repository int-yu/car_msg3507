#ifndef HARDWARE_MOTOR_ENCODER_STEPPER_H
#define HARDWARE_MOTOR_ENCODER_STEPPER_H

#include <stdint.h>

/*
 * Internal bridge between Stepper.c and the shared GPIOA encoder ISR.
 * Application code must use Stepper.h instead of calling these functions.
 */
void Encoder_InitStepper(void);
int32_t Encoder_GetStepperCount(void);
void Encoder_SetStepperCount(int32_t count);
uint32_t Encoder_GetStepperTransitionErrors(void);

#endif
