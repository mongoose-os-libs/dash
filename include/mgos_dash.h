/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 */

/*
 * Device management dashboard API.
 */

#ifndef CS_MOS_LIBS_DASH_H_
#define CS_MOS_LIBS_DASH_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Send an RPC request to the dashboard that does not require an answer.
 * Example - report statistical data:
 * ```c
 *    mgos_dash_call_noreply("Dash.Data", "[%d, %d]", value1, value2);
 * ```
 */
void mgos_dash_callf_noreply(const char *method, const char *json_fmt, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CS_MOS_LIBS_DASH_H_ */
