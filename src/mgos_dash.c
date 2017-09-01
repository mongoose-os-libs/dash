#include <stdbool.h>

#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_rpc.h"

static void timer_cb(void *arg) {
  const struct sys_config_dash *cfg = &get_cfg()->dash;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(cfg->server)};
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Dash.Heartbeat"), NULL, NULL,
               &opts, "%M", (json_printf_callback_t) mgos_print_sys_info);
  (void) arg;
}

static void s_debug_write_hook(enum mgos_hook_type type,
                               const struct mgos_hook_arg *arg,
                               void *userdata) {
  const struct sys_config_dash *cfg = &get_cfg()->dash;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(cfg->server)};
  static unsigned s_seq = 0;
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Dash.Log"), NULL, NULL, &opts,
               "{fd:%d, data: %.*Q, t: %.3lf id:%Q, seq:%u}", arg->debug.fd,
               (int) arg->debug.len, arg->debug.data, mg_time(),
               get_cfg()->device.id, s_seq);
  s_seq++;
  (void) type;
  (void) userdata;
}

bool mgos_dash_init(void) {
  const struct sys_config_dash *cfg = &get_cfg()->dash;

  if (!cfg->enable) return true;

  struct mg_rpc_channel_ws_out_cfg chcfg = {
    .server_address = mg_mk_str(cfg->server),
#if MG_ENABLE_SSL
    .ssl_ca_file = mg_mk_str(cfg->ca_file),
#endif
    .reconnect_interval_min = 5,
    .reconnect_interval_max = 60,
    .idle_close_timeout = 0,
  };
  struct mg_rpc_channel *ch = mg_rpc_channel_ws_out(mgos_get_mgr(), &chcfg);
  if (ch == NULL) {
    LOG(LL_ERROR, ("Cannot create dashboard connection"));
    return false;
  }
  mg_rpc_add_channel(mgos_rpc_get_global(), chcfg.server_address, ch, true);
  mgos_hook_register(MGOS_HOOK_DEBUG_WRITE, s_debug_write_hook, NULL);

  if (cfg->interval > 0) {
    mgos_set_timer(cfg->interval * 1000, 1, timer_cb, NULL);
    LOG(LL_INFO, ("Starting %d sec heartbeat timer", cfg->interval));
  }
  return true;
}
