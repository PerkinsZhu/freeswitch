/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Howell Yang <th15817161961@gmail.com>
 *
 *
 * mod_event_redis.c -- Framework Demo Module
 *
 */

#include "mod_event_redis.h"


SWITCH_MODULE_LOAD_FUNCTION(mod_event_redis_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_redis_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_redis_runtime);

SWITCH_MODULE_DEFINITION(mod_event_redis, mod_event_redis_load, mod_event_redis_shutdown, mod_event_redis_runtime);


static void event_handler(switch_event_t* event)
{
	char* event_name = switch_event_get_header_nil(event, "Event-Name");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, " Event-Name: %s\n", event_name);

}

SWITCH_MODULE_LOAD_FUNCTION(mod_event_redis_load)
{
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_redis_shutdown)
{

	switch_event_unbind_callback(event_handler);//卸载监听的回调事件，不然在卸载过程中如果触发了回调事件，会导致错误  
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_event_redis_shutdown:\n--------------------------------\n");
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_redis_runtime)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_event_redis_runtime:\n--------------------------------\n");
	return SWITCH_STATUS_TERM;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */