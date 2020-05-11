
#include <php.h>

#include "arena.h"
#include "array.h"
#include "def.h"
#include "map.h"
#include "message.h"

ZEND_DECLARE_MODULE_GLOBALS(protobuf)

ZEND_BEGIN_MODULE_GLOBALS(protobuf)
  zend_bool keep_descriptor_pool_after_request;
  zval generated_pool;
ZEND_END_MODULE_GLOBALS(protobuf)

zend_function_entry protobuf_functions[] = {
  ZEND_FE_END
};

static const zend_module_dep protobuf_deps[] = {
  ZEND_MOD_OPTIONAL("date")
  ZEND_MOD_END
};

static PHP_MINIT_FUNCTION(protobuf) {
  arena_module_init();
  array_module_init();
  def_module_init();
  map_module_init();
  message_module_init();
  return 0;
}

static PHP_MSHUTDOWN_FUNCTION(protobuf) {
  return 0;
}

zend_module_entry protobuf_module_entry = {
  STANDARD_MODULE_HEADER_EX,
  NULL,
  protobuf_deps,
  "protobuf",     // extension name
  protobuf_functions,       // function list
  PHP_MINIT(protobuf),      // process startup
  PHP_MSHUTDOWN(protobuf),  // process shutdown
  NULL, // PHP_RINIT(protobuf),      // request shutdown
  NULL, // PHP_RSHUTDOWN(protobuf),  // request shutdown
  NULL,                 // extension info
  "3.13.0", // extension version
  STANDARD_MODULE_PROPERTIES,
  /*
  PHP_MODULE_GLOBALS(protobuf),  // globals descriptor
  PHP_GINIT(protobuf),  // globals ctor
  PHP_GSHUTDOWN(protobuf),  // globals dtor
  NULL,  // post deactivate
  STANDARD_MODULE_PROPERTIES_EX
  */
};

ZEND_GET_MODULE(protobuf)
