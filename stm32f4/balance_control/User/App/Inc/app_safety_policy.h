/**
 * @file app_safety_policy.h
 * @brief Hardware-independent classification of system safety events.
 */
#ifndef APP_SAFETY_POLICY_H
#define APP_SAFETY_POLICY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SAFETY_EVENT_LIMIT = 0,
    SAFETY_EVENT_STEPPER_DIAG,
    SAFETY_EVENT_SENSOR_MISMATCH,
    SAFETY_EVENT_ENCODER_LOST,
    SAFETY_EVENT_VISION_LOST,
    SAFETY_EVENT_CHASSIS_LINK_STALE,
    SAFETY_EVENT_CHASSIS_WATCHDOG,
    SAFETY_EVENT_CHASSIS_HARD_FAULT,
    SAFETY_EVENT_START_ACK_FAILED
} app_safety_event_t;

typedef enum {
    SAFETY_POLICY_WARN = 0,
    SAFETY_POLICY_CONTROLLED_STOP,
    SAFETY_POLICY_LATCHED_ESTOP
} app_safety_policy_action_t;

app_safety_policy_action_t App_SafetyPolicy_Classify(
    app_safety_event_t event);

#ifdef __cplusplus
}
#endif
#endif
