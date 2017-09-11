#include <stdbool.h>

#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_rpc.h"

static void mgos_dash_vcallf(const char *service, const char *json_fmt,
                             va_list ap) {
  const struct sys_config_dash *cfg = &get_cfg()->dash;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(cfg->server),
                                  .key = mg_mk_str(cfg->token),
                                  .noqueue = true};
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

static void s_debug_write_hook(enum mgos_hook_type type,
                               const struct mgos_hook_arg *arg,
                               void *userdata) {
  static unsigned s_seq = 0;
  mgos_dash_callf("Dash.Log", "{fd:%d, data: %.*Q, t: %.3lf, seq:%u}",
                  arg->debug.fd, (int) arg->debug.len, arg->debug.data,
                  mg_time(), s_seq);
  s_seq++;
  (void) type;
  (void) userdata;
}

bool mgos_dash_init(void) {
  const struct sys_config_dash *cfg = &get_cfg()->dash;

  if (!cfg->enable) return true;
  if (cfg->server == NULL) {
    LOG(LL_ERROR, ("dash.enable=true but dash.server is not set"));
    return false;
  }

  struct mg_rpc_channel_ws_out_cfg chcfg = {
      .server_address = mg_mk_str(cfg->server),
      .reconnect_interval_min = 5,
      .reconnect_interval_max = 60,
      .idle_close_timeout = 0,
  };
#if MG_ENABLE_SSL
  if (strncmp(cfg->server, "wss://", 6) == 0) {
    chcfg.ssl_ca_file = mg_mk_str(cfg->ca_file);
  }
#endif

  struct mg_rpc_channel *ch = mg_rpc_channel_ws_out(mgos_get_mgr(), &chcfg);
  if (ch == NULL) {
    LOG(LL_ERROR, ("Cannot create dashboard connection"));
    return false;
  }
  mg_rpc_add_channel(mgos_rpc_get_global(), chcfg.server_address, ch, true);

  if (cfg->send_logs) {
    mgos_hook_register(MGOS_HOOK_DEBUG_WRITE, s_debug_write_hook, NULL);
  }

  if (cfg->heartbeat_interval > 0) {
    mgos_set_timer(cfg->heartbeat_interval * 1000, 1, timer_cb, NULL);
    LOG(LL_INFO, ("Starting %d sec heartbeat timer", cfg->heartbeat_interval));
  }
  return true;
}
