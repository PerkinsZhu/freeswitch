#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_eric_load);
// SWITCH_MODULE_RUNTIME_FUNCTION(mod_eric_runtime);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_eric_shutdown);

// extern "C" {
// Actually it explains as followings:
// static const char modname[] = "mod_eric";
// SWITCH_MOD_DECLARE_DATA switch_loadable_module_function_table_t mod_eric_module_interface ={
//  SWITCH_API_VERSION,
//  mod_eric_load,
//  mod_eric_shutdown,
//  mod_eric_runtime(NULL),
//  SMODF_NONE
// }
SWITCH_MODULE_DEFINITION(mod_eric, mod_eric_load, mod_eric_shutdown, NULL);
//}


switch_bool_t fire_my_event(switch_core_session_t *session)
{
	switch_event_t *event = NULL;
	const char *var = "my custom event";
	// switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, "my_event_name") == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Custom-Variable", var);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
		// switch_channel_event_set_data(channel, event);

		switch_event_fire(&event);
	}

	return SWITCH_TRUE;
}

SWITCH_STANDARD_API(task_api_function)
{
	// SWITCH_STANDARD_API have three args:(cmd, session, stream)
	char *mycmd = NULL;
	int argc = 0;
	char *argv[16];
	bzero(argv, sizeof(argv));

	// split cmd and parse
	if (cmd) {
		mycmd = strdup(cmd);
		if (!mycmd) {
			stream->write_function(stream, "Out of memory\n");
			return SWITCH_STATUS_FALSE;
		}

		if (!(argc = switch_split(mycmd, ' ', argv)) || !argv[0]) {
			argc = 0;
			switch_safe_free(mycmd);
			return SWITCH_STATUS_FALSE;
		}
	}

	// parse cmd, brach process
	// 命令  选项  参数
	// task test1 123
	// task test2 456
	if (0 == strcmp("test1", argv[0])) {
		stream->write_function(stream, "task api test1, cmd:%s, session:%p", cmd, session);

	} else if (0 == strcmp("test2", argv[0])) {
		stream->write_function(stream, "task api test2, cmd:%s, session:%p", cmd, session);
	} else {
		stream->write_function(stream, "unknown cmd, cmd:%s, session:%p", cmd, session);
	}

	switch_safe_free(mycmd);

	//执行api的时候 触发 event事件  task test1
	fire_my_event(session);

	return SWITCH_STATUS_SUCCESS;
}

switch_api_interface_t *api_interface = NULL;

// Actually it explains as followings:
// switch_status_t mod_eric_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
SWITCH_MODULE_LOAD_FUNCTION(mod_eric_load)
{
	// init module interface
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


		// register API
	// 第一个参数是一个API指针。
	// 第二个参数是你的命令名称 在命令行执行的命令
	// 第三个参数是你的命令介绍。
	// 第四个参数是你的对应的命令处理函数。
	// 第五个参数是你的命令参数描述字符串。
	SWITCH_ADD_API(api_interface, "task", "task api.eg: task test1 123", task_api_function, "<cmd><args>");


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_eric_shutdown) { return SWITCH_STATUS_SUCCESS; }