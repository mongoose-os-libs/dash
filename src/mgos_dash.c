/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#include "mgos.h"
#include "mgos_rpc.h"
#include "mgos_shadow.h"

static struct mbuf s_dash_src;

static struct mg_rpc_call_opts mkopts(void) {
  struct mg_rpc_call_opts opts = {
      .dst = mg_mk_str(mgos_sys_config_get_dash_server()),
      .key = mg_mk_str(mgos_sys_config_get_dash_token()),
      .src = mg_mk_str_n(s_dash_src.buf, s_dash_src.len),
  };
  return opts;
}

static void s_debug_write_cb(int ev, void *ev_data, void *userdata) {
  static unsigned s_seq = 0;
  const struct mgos_debug_hook_arg *arg = ev_data;
  struct mg_rpc_call_opts opts = mkopts();
  opts.no_queue = true;
  /* Do not specify callback - we're not expecting an answer to the logs. */
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Dash.Log"), NULL, NULL, &opts,
               "{fd:%d, data: %.*Q, t: %.3lf, seq:%u}", arg->fd, (int) arg->len,
               arg->data, mg_time(), s_seq);
  s_seq++;
  (void) ev;
  (void) userdata;
}

static void upd_res_cb(struct mg_rpc *c, void *cb_arg,
                       struct mg_rpc_frame_info *fi, struct mg_str result,
                       int error_code, struct mg_str error_msg) {
  if (error_code != 0) {
    struct mgos_shadow_error ev_data = {.code = error_code,
                                        .message = error_msg};
    mgos_event_trigger(MGOS_SHADOW_UPDATE_REJECTED, &ev_data);
  } else {
    mgos_event_trigger(MGOS_SHADOW_UPDATE_ACCEPTED, NULL);
  }
  (void) c;
  (void) cb_arg;
  (void) fi;
  (void) result;
}

static void shadow_update_cb(int ev, void *ev_data, void *userdata) {
  struct mgos_shadow_update_data *data = ev_data;
  char *fmt = NULL;
  if (data->version == 0) {
    mg_asprintf(&fmt, 0, "{state: %s}", data->json_fmt);
  } else {
    mg_asprintf(&fmt, 0, "{version: %llu, state: %s}", data->version,
                data->json_fmt);
  }
  struct mg_rpc_call_opts opts = mkopts();
  mg_rpc_vcallf(mgos_rpc_get_global(), mg_mk_str("Dash.Shadow.Update"),
                upd_res_cb, NULL, &opts, fmt, data->ap);
  free(fmt);
  (void) ev;
  (void) userdata;
}

static void get_res_cb(struct mg_rpc *c, void *cb_arg,
                       struct mg_rpc_frame_info *fi, struct mg_str result,
                       int error_code, struct mg_str error_msg) {
  if (error_code != 0) {
    struct mgos_shadow_error ev_data = {.code = error_code,
                                        .message = error_msg};
    mgos_event_trigger(MGOS_SHADOW_GET_REJECTED, &ev_data);
  } else {
    mgos_event_trigger(MGOS_SHADOW_GET_ACCEPTED, &result);
  }
  (void) c;
  (void) cb_arg;
  (void) fi;
  (void) result;
}

static void shadow_get_cb(int ev, void *ev_data, void *userdata) {
  struct mg_rpc_call_opts opts = mkopts();
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Dash.Shadow.Get"), get_res_cb,
               NULL, &opts, NULL);
  (void) ev;
  (void) ev_data;
  (void) userdata;
}

static void shadow_delta_cb(struct mg_rpc_request_info *ri, void *cb_arg,
                            struct mg_rpc_frame_info *fi, struct mg_str args) {
  mgos_event_trigger(MGOS_SHADOW_UPDATE_DELTA, &args);
  mg_rpc_send_responsef(ri, "%B", 1);
  (void) ri;
  (void) cb_arg;
  (void) fi;
}

static void heartbeat_cb(struct mg_rpc *c, void *cb_arg,
                         struct mg_rpc_frame_info *fi, struct mg_str result,
                         int error_code, struct mg_str error_msg) {
  mgos_event_trigger(MGOS_SHADOW_CONNECTED, NULL);
  (void) c;
  (void) cb_arg;
  (void) fi;
  (void) result;
  (void) error_msg;
  (void) error_code;
}

/* When we're connected to the server, send our info */
static void rpc_ev_cb(int ev, void *ev_data, void *userdata) {
  struct mg_str *dst = (struct mg_str *) ev_data;
  if (ev == MGOS_RPC_EV_CHANNEL_OPEN &&
      mg_vcmp(dst, mgos_sys_config_get_dash_server()) == 0) {
    struct mg_rpc_call_opts opts = mkopts();
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Dash.Heartbeat"),
                 heartbeat_cb, NULL, &opts, "%M",
                 (json_printf_callback_t) mgos_print_sys_info);
  }
  (void) userdata;
}

static void stats_timer_cb(void *arg) {
  struct mg_rpc_call_opts opts = mkopts();
  opts.no_queue = true;
  mg_rpc_callf(
      mgos_rpc_get_global(), mg_mk_str("Dash.Shadow.Update"), NULL, NULL, &opts,
      "{state:{reported:{stats:{ram_total:%lu,ram_free:%lu,ram_lwm:%lu}}}}",
      mgos_get_heap_size(), mgos_get_free_heap_size(),
      mgos_get_min_free_heap_size());
  (void) arg;
}

bool mgos_dash_init(void) {
  if (!mgos_sys_config_get_dash_enable()) return true;
  const struct mg_str token = mg_mk_str(mgos_sys_config_get_dash_token());
  if (mgos_sys_config_get_dash_server() == NULL || token.len == 0) {
    LOG(LL_ERROR, ("dash.enable=true but dash.server and/or dash.token "
                   "is not set"));
    return false;
  }

  mbuf_init(&s_dash_src, 0);
  /*
   * Extract ID portion: it's the first part of the token, or, if the token does
   * not have a dash in it, first 8 chars (for compat with existing accounts).
   */
  const char *dash = mg_strchr(token, '-');
  mbuf_append(&s_dash_src, token.p,
              (dash != NULL ? dash - token.p : MIN((int) token.len, 8)));
  mbuf_append(&s_dash_src, "-", 1);
  mbuf_append(&s_dash_src, mgos_sys_config_get_device_id(),
              strlen(mgos_sys_config_get_device_id()));
  mg_rpc_add_local_id(mgos_rpc_get_global(),
                      mg_mk_str_n(s_dash_src.buf, s_dash_src.len));

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

  mgos_event_add_handler(MGOS_RPC_EV_CHANNEL_OPEN, rpc_ev_cb, NULL);

  /* Dash shadow initialisation */
  const char *impl = mgos_sys_config_get_shadow_lib();
  if (impl != NULL && strcmp(impl, "dash") != 0) {
    LOG(LL_INFO, ("shadow.lib=%s, not initialising dash shadow", impl));
  } else {
    struct mg_rpc *r = mgos_rpc_get_global();
    mg_rpc_add_handler(r, "Shadow.Delta", NULL, shadow_delta_cb, NULL);
    mgos_event_add_handler(MGOS_SHADOW_GET, shadow_get_cb, NULL);
    mgos_event_add_handler(MGOS_SHADOW_UPDATE, shadow_update_cb, NULL);
    if (mgos_sys_config_get_dash_stats_interval() > 0) {
      mgos_set_timer(mgos_sys_config_get_dash_stats_interval() * 1000,
                     MGOS_TIMER_REPEAT, stats_timer_cb, NULL);
      LOG(LL_INFO, ("Starting %d sec stats timer",
                    mgos_sys_config_get_dash_stats_interval()));
    }
  }

  LOG(LL_INFO,
      ("Dash init ok (server: %s, ID: %.*s)", mgos_sys_config_get_dash_server(),
       (int) s_dash_src.len, s_dash_src.buf));

  return true;
}
