/*
 * mod_vhost_reserve - Reserve client slots for Apache 2.2
 *
 * Copyright(c) 2014, KOJIMA Kazunori
 *
 * License: Apache License, Version 2.0
 *
 */

#define CORE_PRIVATE

#include "httpd.h"
#include "http_config.h"
#include "http_connection.h"
#include "ap_mpm.h"
#include "http_log.h"
#include "scoreboard.h"

int server_limit;
int thread_limit;
int reserve_slots;
module AP_MODULE_DECLARE_DATA vhost_reserve_module;

typedef enum {
  VHOST_LIMIT_ENABLE,
  VHOST_LIMIT_DISABLE
} vhost_limit_mode;

typedef struct vhost_limit_conf {
  vhost_limit_mode mode;
} vhost_limit_conf;

#define ERR_NEED_EXTENDSTATUS "ExtendedStatus must be set to On for mod_vhost_reserve to be available"

static const char *set_reserve_slots(cmd_parms *cmd, void *dummy, const char *arg)
{
  const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
  if (err != NULL) {
    return err;
  }
  if (!ap_extended_status)
    return ERR_NEED_EXTENDSTATUS;
  reserve_slots = atoi(arg);
  return NULL;
}

static const char *set_vhost_limit_enabled(cmd_parms *cmd, void *dummy, const int arg)
{
  vhost_limit_conf *conf;

  if (!ap_extended_status)
    return ERR_NEED_EXTENDSTATUS;

  conf = (vhost_limit_conf *)ap_get_module_config(cmd->server->module_config, &vhost_reserve_module);

  if (arg) {
    conf->mode = VHOST_LIMIT_ENABLE;
  } else {
    conf->mode = VHOST_LIMIT_DISABLE;
  }

  return NULL;
}

static const char *check_extended_status(cmd_parms *cmd, void *dummy, int arg)
{
  const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
  if (err != NULL) {
    return err;
  }
  ap_extended_status = arg;
  return NULL;
}

static const command_rec vhost_reserve_cmds[] = {
  AP_INIT_FLAG("ExtendedStatus", check_extended_status, NULL, RSRC_CONF, "\"On\" to enable extended status information, \"Off\" to disable"),
  AP_INIT_FLAG("VhostLimitEnabled", set_vhost_limit_enabled, NULL, RSRC_CONF, "\"On\" to enable limit this virtual host"),
  AP_INIT_TAKE1("ReserveSlots", set_reserve_slots, NULL, RSRC_CONF, "'ReserveSlots' followed by non-negative integer"),
  {NULL}
};

static int vhost_reserve_handler(conn_rec *c)
{
  int j, i, busy = 0;
  worker_score *ws_record;
  process_score *ps_record;

  vhost_limit_conf *conf;
  conf = (vhost_limit_conf *)ap_get_module_config(c->base_server->module_config, &vhost_reserve_module);

  if (conf->mode == VHOST_LIMIT_DISABLE) {
    return DECLINED; 
  }

  if (!ap_exists_scoreboard_image()) {
    ap_log_error(APLOG_MARK,APLOG_NOTICE|APLOG_NOERRNO, 0 ,NULL, "mod_vhost_reserve unavailable in inetd mode");
    return HTTP_INTERNAL_SERVER_ERROR;
  }

  for (i = 0; i < server_limit; ++i) {
    for (j = 0; j < thread_limit; ++j) {
      ps_record = ap_get_scoreboard_process(i);
      ws_record = ap_get_scoreboard_worker(i, j);

      if (ps_record->generation != ap_my_generation)
        continue;

      switch(ws_record->status) {
        case SERVER_BUSY_READ:
        case SERVER_BUSY_WRITE:
        case SERVER_BUSY_LOG:
        case SERVER_BUSY_DNS:
        case SERVER_BUSY_KEEPALIVE:
          busy++;
          break;
      }
    }
  }

  if ((busy + reserve_slots) >= (server_limit * thread_limit)) {
    ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, "[client %s] Reached vhost limit", c->remote_ip);
    return HTTP_SERVICE_UNAVAILABLE;
  }

  return DECLINED;
}

static void *vhost_reserve_config(apr_pool_t *p, server_rec *s)
{
  vhost_limit_conf *conf;
  conf = (vhost_limit_conf *) apr_pcalloc(p, sizeof(vhost_limit_conf));
  conf->mode = VHOST_LIMIT_DISABLE;
  return conf;
}

static int vhost_reserve_init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
  ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
  ap_mpm_query(AP_MPMQ_HARD_LIMIT_DAEMONS, &server_limit);
  return OK;
}

static void register_hooks(apr_pool_t *pool)
{
  ap_hook_process_connection(vhost_reserve_handler, NULL, NULL, APR_HOOK_REALLY_FIRST);
  ap_hook_post_config(vhost_reserve_init, NULL, NULL, APR_HOOK_REALLY_FIRST);
}

module AP_MODULE_DECLARE_DATA vhost_reserve_module =
{
  STANDARD20_MODULE_STUFF,
  NULL,                     /* create per-dir config */
  NULL,                     /* merge per-dir config */
  vhost_reserve_config,     /* create per-server config */
  NULL,                     /* merge per-server config */
  vhost_reserve_cmds,       /* table of config file commands */
  register_hooks            /* handlers registration */
};

