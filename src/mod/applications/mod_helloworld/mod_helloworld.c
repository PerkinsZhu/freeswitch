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

// 加载配置文件开始
static struct
{
	char* hostname;      /* 数据库服务器地址 */
	char* dbname;      /* 数据库实例名 */
	int	  port;      /* 数据库端口 */
	char* user;      /* 数据库用户 */
	char* password;      /* 数据库密码 */
} db_globals_t;



static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-hostname", CONFIG_RELOAD, &db_globals_t.hostname, NULL, "localhost", "Hostname for db server"), /* 字符串用这个 */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-name", CONFIG_RELOAD, &db_globals_t.dbname, NULL, "xcroute", "dbname for db server"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-user", CONFIG_RELOAD, &db_globals_t.user, NULL, "postgres", "user for db server"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-password", CONFIG_RELOAD, &db_globals_t.password, NULL, "abc123", "password for db server"),
	SWITCH_CONFIG_ITEM("db-port", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &db_globals_t.port, (void*)5432, NULL,NULL, NULL), /* 数字用这个 */
	SWITCH_CONFIG_ITEM_END()
};
// 加载配置文件结束


}

SWITCH_MODULE_LOAD_FUNCTION(mod_helloworld_load)
{
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}



	//load模块里写下
	//注意这里的 helloworld.conf 要和 xml文件里面的 <configuration name="helloworld.conf"/> 保持一致
	res = switch_xml_config_parse_module_settings("helloworld.conf", SWITCH_FALSE, instructions);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "res %d !\n", res);
	if (res != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "load configure failed!\n");
		return SWITCH_STATUS_FALSE;
	}
	//输出加载的 配置文件参数
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hostname: %s \nport: %d", db_globals_t.hostname, db_globals_t.port);



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
