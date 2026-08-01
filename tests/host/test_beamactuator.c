#include "Application/Control/BeamActuator.h"
#include "Hardware/Motor/Stepper.h"
#include "tests/host/test_assert.h"

static float s_lastTargetDeg;
static uint8_t s_enableCalls;
static uint8_t s_setPositionCalls;
static uint16_t s_trackCalls;
static Stepper_Result_t s_trackResult;
static Stepper_Status_t s_stepperStatus;

Stepper_Result_t Stepper_Enable(bool enable)
{
    if (enable)
    {
        s_enableCalls++;
    }
    return STEPPER_RESULT_OK;
}

Stepper_Result_t Stepper_SetCurrentPosition(float degrees)
{
    (void)degrees;
    s_setPositionCalls++;
    return STEPPER_RESULT_OK;
}

Stepper_Result_t Stepper_TrackToAngle(
    float degrees, const Stepper_Profile_t *profile)
{
    (void)profile;
    s_trackCalls++;
    s_lastTargetDeg = degrees;
    return s_trackResult;
}

void Stepper_GetStatus(Stepper_Status_t *status)
{
    *status = s_stepperStatus;
}

static void reset_fakes(void)
{
    s_lastTargetDeg = 0.0f;
    s_enableCalls = 0U;
    s_setPositionCalls = 0U;
    s_trackCalls = 0U;
    s_trackResult = STEPPER_RESULT_OK;
    s_stepperStatus.enabled = true;
    s_stepperStatus.ready = true;
    s_stepperStatus.busy = false;
    s_stepperStatus.pwmValid = true;
    s_stepperStatus.absoluteAngleDeg = 173.5f;
    BeamActuator_Init();
}

static void test_power_on_angle_becomes_horizontal_zero(void)
{
    reset_fakes();

    CHECK_NEAR(BeamActuator_TuneGearRatio,
               BEAM_ACTUATOR_GEAR_RATIO, 0.001f);
    CHECK(BeamActuator_IsZeroCalibrated() == 0U);
    CHECK(s_enableCalls == 1U);
    CHECK(s_setPositionCalls == 0U);
    BeamActuator_Update(0.01f);
    CHECK(BeamActuator_IsZeroCalibrated() != 0U);
    CHECK_NEAR(BeamActuator_TuneZeroOffsetDeg, 173.5f, 0.001f);
    CHECK_NEAR(s_lastTargetDeg, 173.5f, 0.001f);
}

static void test_waits_for_valid_stepper_reference(void)
{
    reset_fakes();
    s_stepperStatus.ready = false;
    s_stepperStatus.pwmValid = false;

    BeamActuator_Update(0.01f);

    CHECK(BeamActuator_IsZeroCalibrated() == 0U);
    CHECK(s_trackCalls == 0U);
}

static void test_positive_ball_correction_raises_stepper(void)
{
    reset_fakes();

    /* Positive ball error produces negative controller tilt. */
    BeamActuator_SetTiltDeg(-2.0f);
    BeamActuator_Update(0.01f);

    CHECK(s_lastTargetDeg > 173.5f);
    CHECK_NEAR(s_lastTargetDeg,
               173.5f +
                   2.0f * BEAM_ACTUATOR_GEAR_RATIO,
               0.001f);
}

static void test_large_tilt_is_not_clamped_by_actuator(void)
{
    reset_fakes();

    BeamActuator_SetTiltDeg(-30.0f);
    BeamActuator_Update(1.0f);

    CHECK_NEAR(BeamActuator_GetRequestedTiltDeg(), -30.0f, 0.001f);
    CHECK_NEAR(BeamActuator_GetTiltDeg(), -30.0f, 0.001f);
    CHECK_NEAR(s_lastTargetDeg,
               173.5f +
                   30.0f * BEAM_ACTUATOR_GEAR_RATIO,
               0.001f);
}

static void test_bottom_limit_result_does_not_freeze_applied_tilt(void)
{
    reset_fakes();

    s_trackResult = STEPPER_RESULT_LIMIT;
    BeamActuator_SetTiltDeg(-30.0f);
    BeamActuator_Update(1.0f);

    CHECK_NEAR(BeamActuator_GetRequestedTiltDeg(), -30.0f, 0.001f);
    CHECK_NEAR(BeamActuator_GetTiltDeg(), -30.0f, 0.001f);
}

int main(void)
{
    test_power_on_angle_becomes_horizontal_zero();
    test_waits_for_valid_stepper_reference();
    test_positive_ball_correction_raises_stepper();
    test_large_tilt_is_not_clamped_by_actuator();
    test_bottom_limit_result_does_not_freeze_applied_tilt();

    if (s_failures == 0)
    {
        printf("test_beamactuator: ALL PASS\n");
        return 0;
    }
    printf("test_beamactuator: %d FAILURE(S)\n", s_failures);
    return 1;
}
