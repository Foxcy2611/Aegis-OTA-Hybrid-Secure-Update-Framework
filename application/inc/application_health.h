#ifndef AEGIS_APPLICATION_HEALTH_H
#define AEGIS_APPLICATION_HEALTH_H

/* IWDG Trial và confirmation chỉ được điều khiển từ Application healthy. */

#include <stdbool.h>

/* Bật IWDG sớm nếu Metadata cho biết đây là candidate Trial Boot. */
bool ApplicationHealth_BeginTrialWatchdog(void);

/* Gửi TRIAL_CONFIRM sau khi các self-check tối thiểu đã PASS. */
bool ApplicationHealth_ConfirmTrialIfNeeded(void);

#endif /* AEGIS_APPLICATION_HEALTH_H */
