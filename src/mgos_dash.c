/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#include <stdbool.h>

#include "mgos.h"
#include "mgos_rpc.h"

static void mgos_dash_vcallf(const char *service, const char *json_fmt,
                             va_list ap) {
  struct mg_rpc_call_opts opts = {
      .dst = mg_mk_str(mgos_sys_config_get_dash_server()),
      .key = mg_mk_str(mgos_sys_config_get_dash_token()),
      .no_queue = true,
  };
  mg_rpc_vcallf(mgos_rpc_get_global(), mg_mk_str(service), NULL, NULL, &opts,
                json_fmt, ap);
}

static void mgos_dash_callf(const char *service, const char *json_fmt, ...) {
  va_list ap;
  va_start(ap, json_fmt);
  mgos_dash_vcallf(service, json_fmt, ap);
  va_end(ap);
}

void mgos_dash_send_data(const char *json_fmt, ...) {
  va_list ap;
  va_start(ap, json_fmt);
  mgos_dash_vcallf("Dash.Data", json_fmt, ap);
  va_end(ap);
}

static void timer_cb(void *arg) {
  mgos_dash_callf("Dash.Heartbeat", "%M",
                  (json_printf_callback_t) mgos_print_sys_info);
  (void) arg;
}

static void s_debug_write_cb(int ev, void *ev_data, void *userdata) {
  const struct mgos_debug_hook_arg *arg =
      (const struct mgos_debug_hook_arg *) ev_data;
  static unsigned s_seq = 0;
  mgos_dash_callf("Dash.Log", "{fd:%d, data: %.*Q, t: %.3lf, seq:%u}", arg->fd,
                  (int) arg->len, arg->data, mg_time(), s_seq);
  s_seq++;
  (void) ev;
  (void) userdata;
}

static void s_ota_cb(int ev, void *ev_data, void *userdata) {
  const struct mgos_ota_status *s = (const struct mgos_ota_status *) ev_data;
  mgos_dash_callf("Dash.OTAState", "{state: %Q, msg: %Q}",
                  mgos_ota_state_str(s->state), s->msg);
  (void) ev;
  (void) userdata;
}

bool mgos_dash_init(void) {
  if (!mgos_sys_config_get_dash_enable()) return true;
  if (mgos_sys_config_get_dash_server() == NULL) {
    LOG(LL_ERROR, ("dash.enable=true but dash.server is not set"));
    return false;
  }

  struct mg_rpc_channel_ws_out_cfg chcfg = {
      .server_address = mg_mk_str(mgos_sys_config_get_dash_server()),
      .reconnect_interval_min = 5,
      .reconnect_interval_max = 60,
      .idle_close_timeout = 0,
  };
#if MG_ENABLE_SSL
  if (strncmp(mgos_sys_config_get_dash_server(), "wss://", 6) == 0) {
    chcfg.ssl_ca_file = mg_mk_str(mgos_sys_config_get_dash_ca_file());
  }
#endif

  struct mg_rpc_channel *ch = mg_rpc_channel_ws_out(mgos_get_mgr(), &chcfg);
  if (ch == NULL) {
    LOG(LL_ERROR, ("Cannot create dashboard connection"));
    return false;
  }
  mg_rpc_add_channel(mgos_rpc_get_global(), chcfg.server_address, ch);

  if (mgos_sys_config_get_dash_send_logs()) {
    mgos_event_add_handler(MGOS_EVENT_LOG, s_debug_write_cb, NULL);
  }

  if (mgos_sys_config_get_dash_heartbeat_interval() > 0) {
    mgos_set_timer(mgos_sys_config_get_dash_heartbeat_interval() * 1000, 1,
                   timer_cb, NULL);
    LOG(LL_INFO, ("Starting %d sec heartbeat timer",
                  mgos_sys_config_get_dash_heartbeat_interval()));
  }

  mgos_event_add_handler(MGOS_EVENT_OTA_STATUS, s_ota_cb, NULL);

  /* If we're running an uncommited firmware, report that. */
  if (!mgos_upd_is_committed()) {
    struct mg_rpc_call_opts opts = {
        .dst = mg_mk_str(mgos_sys_config_get_dash_server()),
        .key = mg_mk_str(mgos_sys_config_get_dash_token())};
    /* Dont use mgos_dash_callf() in order to queue this request. */
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Dash.OTAState"), NULL, NULL,
                 &opts, "{state: %Q, msg: %Q}", "boot",
                 "Boot after OTA, waiting for commit");
  }

  return true;
}
