#include "boot_update.h"
#include "watchdog.h"

int main(void){
    (void)BootUpdate_Run();

    /*
     * Recovery baseline: giữ SWD ổn định. IWDG có thể còn chạy sau khi
     * candidate treo nên phải feed; nếu WFI ở đây thiết bị sẽ reset liên tục.
     */
    for(;;){
        Watchdog_Feed();
    }
}
