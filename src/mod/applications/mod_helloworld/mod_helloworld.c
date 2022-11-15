#include "mod_helloworld.h"



SWITCH_MODULE_LOAD_FUNCTION(mod_helloworld_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_helloworld_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_helloworld_runtime);

SWITCH_MODULE_DEFINITION(mod_helloworld, mod_helloworld_load, mod_helloworld_shutdown, mod_helloworld_runtime);


static void event_handler(switch_event_t* event)
{
	char* event_name = switch_event_get_header_nil(event, "Event-Name");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, " Event-Name: %s\n", event_name);

}

static void load_config()
{
	switch_xml_t x_queues, x_queue, cfg, x_agents, x_agent, x_tiers;
	switch_xml_t xml = NULL;
	switch_event_t* event = NULL;
	switch_event_t* params = NULL;

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "CC-Queue", queue_name);

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		goto end;
	}


}

SWITCH_MODULE_LOAD_FUNCTION(mod_helloworld_load)
{
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_helloworld_shutdown)
{

	switch_event_unbind_callback(event_handler);//卸载监听的回调事件，不然在卸载过程中如果触发了回调事件，会导致错误  
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_helloworld_shutdown:\n--------------------------------\n");
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_helloworld_runtime)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_helloworld_runtime:\n--------------------------------\n");
	return SWITCH_STATUS_TERM;
}
