#include "mod_book.h"





SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_book_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_book_load);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_book_runtime);
SWITCH_MODULE_DEFINITION(mod_book,mod_book_load, mod_book_shutdown, NULL);


SWITCH_MODULE_LOAD_FUNCTION(mod_book_load)
{
	// init module interface
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_book_shutdown) { return SWITCH_STATUS_SUCCESS; }
