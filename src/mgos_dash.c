#include <stdbool.h>

#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_rpc.h"

static void timer_cb(void *arg) {
  char buf[40], *topic = get_cfg()->dash.topic;
  if (topic == NULL) {
    snprintf(buf, sizeof(buf), "/%s/dash", get_cfg()->device.id);
    topic = buf;
  }
  struct mbuf m;
  struct json_out out = JSON_OUT_MBUF(&m);
  mbuf_init(&m, 0);
  mgos_print_sys_info(&out);
  bool ok = mgos_mqtt_pub(topic, m.buf, m.len, 1, false);
  mbuf_free(&m);
  LOG(LL_INFO, ("%d, %p", ok, arg));
}

bool mgos_dash_init(void) {
  const struct sys_config_dash *cfg = &get_cfg()->dash;
  if (!cfg->enable) return true;
  if (cfg->interval > 0) {
    mgos_set_timer(cfg->interval * 1000, 1, timer_cb, NULL);
    LOG(LL_INFO, ("Starting %d sec heartbeat timer", cfg->interval));
  }
  return true;
}
