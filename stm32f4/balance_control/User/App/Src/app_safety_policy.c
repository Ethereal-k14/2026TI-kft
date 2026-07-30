/**
 * @file app_safety_policy.c
 * @brief Consequence-based safety classification without BSP dependencies.
 */
#include "app_safety_policy.h"

app_safety_policy_action_t App_SafetyPolicy_Classify(
    app_safety_event_t event)
{
    switch (event) {
    case SAFETY_EVENT_VISION_LOST:
    case SAFETY_EVENT_START_ACK_FAILED:
        return SAFETY_POLICY_CONTROLLED_STOP;

    case SAFETY_EVENT_CHASSIS_LINK_STALE:
    case SAFETY_EVENT_CHASSIS_WATCHDOG:
        return SAFETY_POLICY_WARN;

    case SAFETY_EVENT_LIMIT:
    case SAFETY_EVENT_STEPPER_DIAG:
    case SAFETY_EVENT_SENSOR_MISMATCH:
    case SAFETY_EVENT_ENCODER_LOST:
    case SAFETY_EVENT_CHASSIS_HARD_FAULT:
    default:
        return SAFETY_POLICY_LATCHED_ESTOP;
    }
}
