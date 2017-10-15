/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

/*
 * Device management dashboard API.
 *
 * See https://mongoose-os.com/docs/reference/dashboard.html for more
 * information on device management dashboard.
 */

#ifndef CS_MOS_LIBS_DASH_SRC_MGOS_CRON_H_
#define CS_MOS_LIBS_DASH_SRC_MGOS_CRON_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Send a JSON string representing measured numeric data to the dashboard.
 * Example:
 * ```c
 * mgos_dash_send_data("{foo: %f, bar: %f}", 1.23, 4.56);
 * ```
 */
void mgos_dash_send_data(const char *json_fmt, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
