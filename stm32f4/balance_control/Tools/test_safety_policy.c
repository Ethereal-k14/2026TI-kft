#include "app_safety_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_LIMIT) ==
           SAFETY_POLICY_LATCHED_ESTOP);
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_STEPPER_DIAG) ==
           SAFETY_POLICY_LATCHED_ESTOP);
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_ENCODER_LOST) ==
           SAFETY_POLICY_LATCHED_ESTOP);
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_VISION_LOST) ==
           SAFETY_POLICY_CONTROLLED_STOP);
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_START_ACK_FAILED) ==
           SAFETY_POLICY_CONTROLLED_STOP);
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_CHASSIS_LINK_STALE) ==
           SAFETY_POLICY_WARN);
    assert(App_SafetyPolicy_Classify(SAFETY_EVENT_CHASSIS_WATCHDOG) ==
           SAFETY_POLICY_WARN);
    assert(App_SafetyPolicy_Classify((app_safety_event_t)99) ==
           SAFETY_POLICY_LATCHED_ESTOP);
    puts("safety policy tests: PASS");
    return 0;
}
