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

// ���������ļ���ʼ
static struct
{
	char* hostname;      /* ���ݿ��������ַ */
	char* dbname;      /* ���ݿ�ʵ���� */
	int	  port;      /* ���ݿ�˿� */
	char* user;      /* ���ݿ��û� */
	char* password;      /* ���ݿ����� */
} db_globals_t;



static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-hostname", CONFIG_RELOAD, &db_globals_t.hostname, NULL, "localhost", "Hostname for db server"), /* �ַ�������� */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-name", CONFIG_RELOAD, &db_globals_t.dbname, NULL, "xcroute", "dbname for db server"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-user", CONFIG_RELOAD, &db_globals_t.user, NULL, "postgres", "user for db server"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-password", CONFIG_RELOAD, &db_globals_t.password, NULL, "abc123", "password for db server"),
	SWITCH_CONFIG_ITEM("db-port", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &db_globals_t.port, (void*)5432, NULL,NULL, NULL), /* ��������� */
	SWITCH_CONFIG_ITEM_END()
};
// ���������ļ�����


}

SWITCH_MODULE_LOAD_FUNCTION(mod_helloworld_load)
{
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}



	//loadģ����д��
	//ע������� helloworld.conf Ҫ�� xml�ļ������ <configuration name="helloworld.conf"/> ����һ��
	res = switch_xml_config_parse_module_settings("helloworld.conf", SWITCH_FALSE, instructions);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "res %d !\n", res);
	if (res != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "load configure failed!\n");
		return SWITCH_STATUS_FALSE;
	}
	//������ص� �����ļ�����
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hostname: %s \nport: %d", db_globals_t.hostname, db_globals_t.port);



	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_helloworld_shutdown)
{

	switch_event_unbind_callback(event_handler);//ж�ؼ����Ļص��¼�����Ȼ��ж�ع�������������˻ص��¼����ᵼ�´���  
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_helloworld_shutdown:\n--------------------------------\n");
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_helloworld_runtime)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_helloworld_runtime:\n--------------------------------\n");
	return SWITCH_STATUS_TERM;
}
