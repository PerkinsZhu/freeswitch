#include <switch.h>



SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_testa_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_testa_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_testa_load);

SWITCH_MODULE_DEFINITION(mod_testa, mod_testa_load, mod_testa_shutdown, NULL);

switch_status_t res;
switch_api_interface_t *api_interface = NULL;
switch_application_interface_t *app_interface = NULL;

// 加载配置文件开始
static struct {
	char *hostname; /* 数据库服务器地址 */
	char *dbname;	/* 数据库实例名 */
	int port;		/* 数据库端口 */
	char *user;		/* 数据库用户 */
	char *password; /* 数据库密码 */
} db_globals_t;

static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options
	   structure */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-hostname", CONFIG_RELOAD, &db_globals_t.hostname, NULL, "localhost",
									 "Hostname for db server"), /* 字符串用这个 */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-name", CONFIG_RELOAD, &db_globals_t.dbname, NULL, "xcroute",
									 "dbname for db server"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-user", CONFIG_RELOAD, &db_globals_t.user, NULL, "postgres",
									 "user for db server"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-password", CONFIG_RELOAD, &db_globals_t.password, NULL, "abc123",
									 "password for db server"),
	SWITCH_CONFIG_ITEM("db-port", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &db_globals_t.port, (void *)5432, NULL, NULL,
					   NULL), /* 数字用这个 */
	SWITCH_CONFIG_ITEM_END()};
// 加载配置文件结束

//挂机事件
void task_event_channel_hangup_complete(switch_event_t *event)
{
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	const char *event_name = switch_event_get_header(event, "Event-Name");
	const char *call_dir = switch_event_get_header(event, "Call-Direction");
	const char *task_str = switch_event_get_header(event, "variable_task_str"); //获取在task_app_function中设置的 task_str通道变量

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					  "task_event_channel_hangup_complete, event_name=%s, uuid=%s, call_dir=%s, task_str=%s\n", event_name, uuid,
					  call_dir,
					  task_str);
}

// 处理事件监听
static void event_handler(switch_event_t *event)
{
	char* event_name = switch_event_get_header_nil(event, "Event-Name");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "====== Event-Name: %s\n", event_name);

	switch (event->event_id) {
	case SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE:
		task_event_channel_hangup_complete(event);
		break;
	default:
		break;
	}
}


// 自定义event事件
// 订阅事件的命令(通过 fs_cli进入 )   /event plain CUSTOM my_event_name      /events plain all
// 取消订阅所有事件   /nixevent plain all           /nixevent all
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



// media bug回调函数
static switch_bool_t my_media_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);

	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = {0};
	frame.data = data;
	frame.buflen = sizeof(data);

	const char *robot_id = (const char *)user_data;
	if (robot_id == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "mymedia user data is null!");
		return SWITCH_FALSE;
	}

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Media bug initialized\n");
		break;

	case SWITCH_ABC_TYPE_READ:
		// 处理读取的音频数据
		if (switch_core_media_bug_read(bug, &frame, SWITCH_FALSE) != SWITCH_STATUS_FALSE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "read frame: data_len=[%d], rate[%d], channels[%d], samples[%d] \n", frame.datalen,
							  frame.rate, frame.channels, frame.samples);
		}
		break;

	case SWITCH_ABC_TYPE_WRITE:
		// 处理写入的音频数据
		break;

	case SWITCH_ABC_TYPE_CLOSE:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Media bug closed\n");
		break;

	default:
		break;
	}

	return SWITCH_TRUE;
}


//测试媒体流获取
static void test_media_bug(switch_core_session_t *session)
{

	//在这里获取媒体流监听
	switch_status_t status;
	switch_media_bug_t *bug;

	status = switch_core_media_bug_add(session, "my_media_bug", NULL, my_media_bug_callback, NULL, 0,
									   SMBF_READ_STREAM | SMBF_WRITE_STREAM, &bug);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add media bug!\n");
		return;
	}

	// 保持bug一段时间
	switch_sleep(1000000);

	switch_core_media_bug_remove(session, bug);

}
	//执行API 方法
SWITCH_STANDARD_API(task_api_function)
{
	// SWITCH_STANDARD_API have three args:(cmd, session, stream)
	char *mycmd = NULL;
	int argc = 0;
	char *argv[16];
	memset(argv,0, sizeof(argv));

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

		//触发获取媒体流测试
		//test_media_bug(session);


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


// 自定义函数：将呼叫转接到指定的坐席
static void transfer_call_to_agent(switch_core_session_t *session, const char *agent_extension)
{
	if (!session || !agent_extension) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid session or agent extension!\n");
		return;
	}

	// 转接到指定的坐席分机
	switch_status_t status = switch_ivr_session_transfer(session, agent_extension, NULL, NULL);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "================ Failed to transfer call to agent %s\n",
						  agent_extension);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "================ Call transferred to agent %s successfully\n",
						  agent_extension);
	}
}



	//执行APP事件，该APP需要在 diaplan中 通过action 标签触发
SWITCH_STANDARD_APP(task_app_function)
{
	switch_channel_t *pchannel = NULL;

	// task_app(session, data);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "task_app_function start, session=%p, data=%s\n", (void *)session, data);

	// export variable task_str for hangup event
	pchannel = switch_core_session_get_channel(session);
	if (NULL != pchannel) {
		//设置了一个通道变量 task_str,在挂断事件中可以获取到设置的 "task_app export variable"
		switch_channel_export_variable(pchannel, "task_str", "task_app export variable", SWITCH_EXPORT_VARS_VARIABLE);

		
		
		
		//把通话转向指定坐席
		const char *agent_extension = switch_channel_get_variable(switch_core_session_get_channel(session), "agent_extension");
		if (agent_extension) {
			transfer_call_to_agent(session, agent_extension);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No agent extension specified!\n");
		}


	}

}

// 定义一个名为 my_dialplan_hunt的标准拨号计划处理函数
SWITCH_STANDARD_DIALPLAN(my_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL; // 声明一个呼叫方扩展结构体指针，用于存储呼叫路由信息
	switch_channel_t *channel = switch_core_session_get_channel(session); // 获取当前呼叫的通道信息

	if (!caller_profile) {											 // 如果呼叫者配置信息为空
		caller_profile = switch_channel_get_caller_profile(channel); // 获取呼叫者的配置信息
	}

	// 记录日志，显示呼叫处理过程中的相关信息
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Processing %s <%s> -> %s in context %s\n",
					  caller_profile->caller_id_name, caller_profile->caller_id_number,
					  caller_profile->destination_number, caller_profile->context);

	// 创建一个名为 "my_dialplan" 的呼叫方扩展，并将其添加到当前会话的呼叫方扩展列表中
	extension = switch_caller_extension_new(session, "my_dialplan", "my_dialplan");

	if (!extension) // 如果创建呼叫方扩展失败
		abort();	// 终止程序

	// 向呼叫方扩展添加一个应用程序，用于在日志中记录信息
	switch_caller_extension_add_application(session, extension, "log", "INFO Hey, I'm in the ，my_dialplan");
	//添加一个action，把通话转接到 1002
	switch_caller_extension_add_application(session, extension, "bridge", "user/1002");

	return extension; // 返回创建的呼叫方扩展结构体指针
}




SWITCH_MODULE_LOAD_FUNCTION(mod_testa_load)
{
	switch_api_interface_t *api_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


	// load模块里写下
	//注意这里的 testa.conf 要和 xml文件里面的 <configuration name="testa.conf"/> 保持一致
	res = switch_xml_config_parse_module_settings("testa.conf", SWITCH_FALSE, instructions);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "res %d !\n", res);

	if (res != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "load configure failed!\n");
		return SWITCH_STATUS_FALSE;
	}
	//输出加载的 配置文件参数
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hostname: %s port: %d", db_globals_t.hostname,
					  db_globals_t.port);

	//绑定监听事件
	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}


	  // register API
	// 第一个参数是一个API指针。
	// 第二个参数是你的命令名称 在命令行执行的命令
	// 第三个参数是你的命令介绍。
	// 第四个参数是你的对应的命令处理函数。
	// 第五个参数是你的命令参数描述字符串。
	SWITCH_ADD_API(api_interface, "task", "task api.eg: task test1 123", task_api_function, "<cmd><args>");

	
    //注册终端命令自动补全
	switch_console_set_complete("add tasktest1 [args]");
	switch_console_set_complete("add tasktest2 [args]");

	// register APP
	SWITCH_ADD_APP(app_interface, "task_app", "task_app", "task_app", task_app_function, "NULL",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);



	/************************拨号计划测试 开始************************/

	 // 拨号计划接口指针
	switch_dialplan_interface_t *dp_interface;

	//表示向名为 dp_interface 的拨号计划接口中添加一个名称为 "my_dialplan" 的自定义拨号计划，
	//并指定了处理匹配的函数为 my_dialplan_hunt。
	SWITCH_ADD_DIALPLAN(dp_interface, "my_dialplan", my_dialplan_hunt);

	//测试方式： 通过命令行发起呼叫： originate user/1002 9999 my_dialplan

	/************************拨号计划测试 结束************************/


	/************************变声测试 开始************************/






	/************************变声测试 结束************************/


	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_testa_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_testa_shutdown)
{
	switch_event_unbind_callback(event_handler); //解绑监听事件
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,  "\n mod_helloworld_shutdown:\n--------------------------------\n");
	switch_xml_config_cleanup(instructions);
	return SWITCH_STATUS_SUCCESS;
}
