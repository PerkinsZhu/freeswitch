#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_eric_load);
// SWITCH_MODULE_RUNTIME_FUNCTION(mod_eric_runtime);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_eric_shutdown);



SWITCH_MODULE_DEFINITION(mod_eric, mod_eric_load, mod_eric_shutdown, NULL);


extern "C" {
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
}

// Actually it explains as followings:
// switch_status_t mod_eric_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
SWITCH_MODULE_LOAD_FUNCTION(mod_eric_load)
{
	// init module interface
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_eric_shutdown) { return SWITCH_STATUS_SUCCESS; }