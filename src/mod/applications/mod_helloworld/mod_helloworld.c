#include "mod_helloworld.h"



SWITCH_MODULE_LOAD_FUNCTION(mod_helloworld_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_helloworld_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_helloworld_runtime);

SWITCH_MODULE_DEFINITION(mod_helloworld, mod_helloworld_load, mod_helloworld_shutdown, mod_helloworld_runtime);


// ���������ļ���ʼ
static struct
{
    char* hostname;      /* ���ݿ��������ַ */
    char* dbname;      /* ���ݿ�ʵ���� */
    int			port;      /* ���ݿ�˿� */
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

//�һ��¼� 
void task_event_channel_hangup_complete(switch_event_t* event)
{
    const char* uuid = switch_event_get_header(event, "Unique-ID");
    const char* call_dir = switch_event_get_header(event, "Call-Direction");
    const char* task_str = switch_event_get_header(event, "variable_task_str"); //��ȡ��task_app_function�����õ� task_strͨ������

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "task_event_channel_hangup_complete, uuid=%s, call_dir=%s, task_str=%s\n", uuid, call_dir, task_str);
}

// �����¼�����
static void event_handler(switch_event_t* event)
{
    // char* event_name = switch_event_get_header_nil(event, "Event-Name");
    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, " Event-Name: %s\n", event_name);

    switch (event->event_id)
    {
    case SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE:
        task_event_channel_hangup_complete(event);
        break;
    default:
        break;
    }

}


//ִ��APP�¼�����APP��Ҫ�� diaplan�� ͨ��action ��ǩ����
SWITCH_STANDARD_APP(task_app_function)
{
    switch_channel_t* pchannel = NULL;

    //task_app(session, data);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "task_app_function start, session=%p, data=%s\n", (void*)session, data);

    //export variable task_str for hangup event
    pchannel = switch_core_session_get_channel(session);
    if (NULL != pchannel)
    {
        //������һ��ͨ������ task_str
        switch_channel_export_variable(pchannel, "task_str", "task_app export variable", SWITCH_EXPORT_VARS_VARIABLE);
    }
}



// �Զ���event�¼�
// �����¼�������(ͨ�� fs_cli���� )   /event plain CUSTOM my_event_name      /events plain all
// ȡ�����������¼�   /nixevent plain all           /nixevent all
switch_bool_t fire_my_event(switch_core_session_t* session)
{
    switch_event_t* event = NULL;
    const char* var = "my custom event";
    // switch_channel_t *channel = switch_core_session_get_channel(session);

    if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, "my_event_name") == SWITCH_STATUS_SUCCESS) {
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Custom-Variable", var);
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
        // switch_channel_event_set_data(channel, event);

        switch_event_fire(&event);
    }

    return SWITCH_TRUE;
}


//ִ��API ����
SWITCH_STANDARD_API(task_api_function)
{
    //SWITCH_STANDARD_API have three args:(cmd, session, stream)
    char* mycmd = NULL;
    int argc = 0;
    char* argv[16];
    bzero(argv, sizeof(argv));

    //split cmd and parse
    if (cmd)
    {
        mycmd = strdup(cmd);
        if (!mycmd)
        {
            stream->write_function(stream, "Out of memory\n");
            return SWITCH_STATUS_FALSE;
        }

        if (!(argc = switch_split(mycmd, ' ', argv)) || !argv[0])
        {
            argc = 0;
            switch_safe_free(mycmd);
            return SWITCH_STATUS_FALSE;
        }
    }

    //parse cmd, brach process
    // ����  ѡ��  ����
    // task test1 123
    // task test2 456
    if (0 == strcmp("test1", argv[0]))
    {
        stream->write_function(stream, "task api test1, cmd:%s, session:%p", cmd, session);

    }
    else if (0 == strcmp("test2", argv[0]))
    {
        stream->write_function(stream, "task api test2, cmd:%s, session:%p", cmd, session);
    }
    else
    {
        stream->write_function(stream, "unknown cmd, cmd:%s, session:%p", cmd, session);
    }

    switch_safe_free(mycmd);

    //ִ��api��ʱ�� ���� event�¼�  task test1
    fire_my_event(session);

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t res;
switch_api_interface_t* api_interface = NULL;
switch_application_interface_t* app_interface = NULL;



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



    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    // register API
      // ��һ��������һ��APIָ�롣
      // �ڶ�������������������� ��������ִ�е����� 
      // ���������������������ܡ�
      // ���ĸ���������Ķ�Ӧ�����������
      // ��������������������������ַ�����
    SWITCH_ADD_API(api_interface, "task", "task api.eg: task test1 123", task_api_function, "<cmd><args>");

    //ע���ն������Զ���ȫ
    switch_console_set_complete("add tasktest1 [args]");
    switch_console_set_complete("add tasktest2 [args]");


    // register APP
    SWITCH_ADD_APP(app_interface, "task_app", "task_app", "task_app", task_app_function, "NULL", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);


    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_helloworld_shutdown)
{

    switch_event_unbind_callback(event_handler);//�������¼�  
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_helloworld_shutdown:\n--------------------------------\n");
    return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_helloworld_runtime)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\n mod_helloworld_runtime:\n--------------------------------\n");
    return SWITCH_STATUS_TERM;
}
