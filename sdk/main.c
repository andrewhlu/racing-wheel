#include "bsp.h"                             /* Board Support Package (BSP) */
#include "qpn_port.h"                                       /* QP-nano port */
#include "racing_wheel.h"                               /* application interface */
#include "xil_cache.h"		                /* Cache Drivers */

static QEvent l_racingWheelQueue[30];  

QActiveCB const Q_ROM Q_ROM_VAR QF_active[] = {
	{ (QActive *)0,                  (QEvent *)0,          0                    },
	{ (QActive *)&AO_RacingWheel,    l_racingWheelQueue,   Q_DIM(l_racingWheelQueue)  }
};

Q_ASSERT_COMPILE(QF_MAX_ACTIVE == Q_DIM(QF_active) - 1);

// Do not edit main, unless you have a really good reason
int main(void) {

	Xil_ICacheInvalidate();
	Xil_ICacheEnable();
	Xil_DCacheInvalidate();
	Xil_DCacheEnable();

	RacingWheel_ctor();
	BSP_init(); // inside of bsp.c, starts out empty!
	QF_run(); // inside of qfn.c
	return 0;
}
