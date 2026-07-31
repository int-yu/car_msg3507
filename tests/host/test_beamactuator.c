#include "Application/Control/BeamActuator.h"
#include "Hardware/Motor/Stepper.h"
#include "tests/host/test_assert.h"

static float s_lastTargetDeg;
static uint8_t s_enableCalls;
static uint8_t s_setPositionCalls;
static Stepper_Result_t s_trackResult;

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
    s_lastTargetDeg = degrees;
    return s_trackResult;
}

static void reset_fakes(void)
{
    s_lastTargetDeg = 0.0f;
    s_enableCalls = 0U;
    s_setPositionCalls = 0U;
    s_trackResult = STEPPER_RESULT_OK;
    BeamActuator_Init();
}

static void test_absolute_horizontal_reference_is_preserved(void)
{
    reset_fakes();

    CHECK_NEAR(BeamActuator_TuneGearRatio,
               BEAM_ACTUATOR_GEAR_RATIO, 0.001f);
    CHECK_NEAR(BeamActuator_TuneZeroOffsetDeg,
               STEPPER_INITIAL_ANGLE_DEG, 0.001f);
    CHECK(s_enableCalls == 1U);
    CHECK(s_setPositionCalls == 0U);
    BeamActuator_Update(0.01f);
    CHECK_NEAR(s_lastTargetDeg, STEPPER_INITIAL_ANGLE_DEG, 0.001f);
}

static void test_positive_ball_correction_raises_stepper(void)
{
    reset_fakes();

    /* Positive ball error produces negative controller tilt. */
    BeamActuator_SetTiltDeg(-2.0f);
    BeamActuator_Update(0.01f);

    CHECK(s_lastTargetDeg > STEPPER_INITIAL_ANGLE_DEG);
    CHECK_NEAR(s_lastTargetDeg,
               STEPPER_INITIAL_ANGLE_DEG +
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
               STEPPER_INITIAL_ANGLE_DEG +
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
    test_absolute_horizontal_reference_is_preserved();
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
