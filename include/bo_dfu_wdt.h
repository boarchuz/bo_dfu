#ifndef BO_DFU_WDT_H
#define BO_DFU_WDT_H

#include "esp_attr.h"
#include "hal/rwdt_ll.h"
#include "soc/rtc_cntl_struct.h"

FORCE_INLINE_ATTR IRAM_ATTR void bo_dfu_wdt_unlock(void)
{
    rwdt_ll_write_protect_disable(&RTCCNTL);
}

FORCE_INLINE_ATTR IRAM_ATTR void bo_dfu_wdt_lock(void)
{
    rwdt_ll_write_protect_enable(&RTCCNTL);
}

FORCE_INLINE_ATTR IRAM_ATTR void bo_dfu_wdt_feed(void)
{
    #if 0
        // This read+modify+write takes so long that it may be disruptive to the USB bus
        rwdt_ll_feed(&RTCCNTL);
    #else
        RTCCNTL.wdt_feed.val = (1 << RTC_CNTL_WDT_FEED_S);
    #endif
}

#endif /* BO_DFU_WDT_H */
