#ifndef racing_wheel_h
#define racing_wheel_h

enum RacingWheelSignals {
    DPAD_UP  = Q_USER_SIG,
    DPAD_LEFT,
    DPAD_RIGHT,
    DPAD_DOWN,
    DPAD_CENTER,
    DPAD_RELEASE_UP,
    DPAD_RELEASE_LEFT,
    DPAD_RELEASE_RIGHT,
    DPAD_RELEASE_DOWN,
    DPAD_RELEASE_CENTER,
    SWITCH_0_OFF,
    SWITCH_0_ON,
    SWITCH_15_OFF,
    SWITCH_15_ON,
    TICK_SIG
};

extern struct RacingWheelTag AO_RacingWheel;

void RacingWheel_ctor();

#endif  
