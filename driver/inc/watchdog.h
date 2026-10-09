#ifndef AEGIS_WATCHDOG_H
#define AEGIS_WATCHDOG_H

/* Primitive IWDG dùng chung; policy Trial Boot thuộc Application/Bootloader. */

typedef enum{
    WATCHDOG_OK = 0U,
    /* IWDG đã được cấu hình và khởi động. */

    WATCHDOG_SYNC_TIMEOUT
    /* Thanh ghi prescaler/reload không đồng bộ xong đúng hạn. */
} Watchdog_Status_t;

/*
 * Cấu hình IWDG với prescaler 256 và reload 4095.
 * Với LSI danh định 40 kHz, timeout xấp xỉ 26 giây.
 */
Watchdog_Status_t Watchdog_StartMaxTimeout(void);

/* Reload counter. Gọi khi IWDG chưa chạy cũng không làm IWDG tự khởi động. */
void Watchdog_Feed(void);

#endif /* AEGIS_WATCHDOG_H */
