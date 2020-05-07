// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "map.h"

#include <Zend/zend_API.h>
#include <Zend/zend_interfaces.h>

#include <ext/spl/spl_iterators.h>

#include "arena.h"
#include "convert.h"
#include "core-upb.h"

//#include "utf8.h"

static void MapFieldIter_make(zval *val, zval *map_field);

// -----------------------------------------------------------------------------
// MapField
// -----------------------------------------------------------------------------

typedef struct MapField {
  zend_object std;
  zval arena;
  upb_map *map;
  upb_fieldtype_t key_type;
  upb_fieldtype_t value_type;
  const zend_class_entry* klass;  // class entry for value message
} MapField;

zend_class_entry *map_field_ce;
static zend_object_handlers map_field_object_handlers;

/**
 * Object handler to create a MapField.
 */
static zend_object* map_field_create(zend_class_entry *class_type) {
  MapField *intern = emalloc(sizeof(MapField));
  zend_object_std_init(&intern->std, class_type);
  intern->std.handlers = &map_field_object_handlers;
  arena_init(&intern->arena);
  // Skip object_properties_init(), we don't allow derived classes.
  return &intern->std;
}

/**
 * Object handler to free a MapField.
 */
static void map_field_free(zend_object* obj) {
  MapField* intern = (MapField*)obj;
  zval_ptr_dtor(&intern->arena);
  zend_object_std_dtor(&intern->std);
}

/**
 * Object handler to get GC references.
 */
static HashTable *map_field_get_gc(zval *object, zval **table, int *n) {
  MapField *intern = (MapField*)Z_OBJ_P(object);
  *table = &intern->arena;
  *n = 1;
  return NULL;
}

/**
 * Constructs an instance of RepeatedField.
 * @param long Key type.
 * @param long Value type.
 * @param string Message/Enum class (message/enum value types only).
 */
PHP_METHOD(MapField, __construct) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());
  upb_arena *arena = arena_get(&intern->arena);
  zend_long key_type, value_type;
  zend_class_entry* klass = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "ll|C", &key_type, &value_type,
                            &klass) != SUCCESS) {
    return;
  }

  intern->key_type = pbphp_dtype_to_type(key_type);
  intern->value_type = pbphp_dtype_to_type(value_type);
  intern->klass = klass;

  // Check that the key type is an allowed type.
  switch (intern->key_type) {
    case UPB_TYPE_INT32:
    case UPB_TYPE_INT64:
    case UPB_TYPE_UINT32:
    case UPB_TYPE_UINT64:
    case UPB_TYPE_BOOL:
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES:
      // These are OK.
      break;
    default:
      zend_error(E_USER_ERROR, "Invalid key type for map.");
  }

  intern->map = upb_map_new(arena, intern->key_type, intern->value_type);
}

PHP_METHOD(MapField, offsetExists) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());
  zval *key;
  upb_msgval upb_key;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) != SUCCESS ||
      !pbphp_tomsgval(key, &upb_key, intern->key_type, intern->klass, NULL)) {
    return;
  }

  RETURN_BOOL(upb_map_get(intern->map, upb_key, NULL));
}

PHP_METHOD(MapField, offsetGet) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());
  zval *key;
  zval ret;
  upb_msgval upb_key, upb_val;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) != SUCCESS ||
      !pbphp_tomsgval(key, &upb_key, intern->key_type, intern->klass, NULL)) {
    return;
  }

  if (!upb_map_get(intern->map, upb_key, &upb_val)) {
    zend_error(E_USER_ERROR, "Given key doesn't exist.");
    return;
  }

  pbphp_tozval(upb_val, &ret, intern->value_type, intern->klass);
  RETURN_ZVAL(&ret, 0, 1);
}

PHP_METHOD(MapField, offsetSet) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());
  upb_arena *arena = arena_get(&intern->arena);
  zval *key, *val;
  upb_msgval upb_key, upb_val;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &key, &val) != SUCCESS ||
      !pbphp_tomsgval(key, &upb_key, intern->key_type, NULL, NULL) ||
      !pbphp_tomsgval(val, &upb_val, intern->value_type, intern->klass, NULL)) {
    return;
  }

  upb_map_set(intern->map, upb_key, upb_val, arena);
}

PHP_METHOD(MapField, offsetUnset) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());
  zval *key;
  upb_msgval upb_key;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) != SUCCESS ||
      !pbphp_tomsgval(key, &upb_key, intern->key_type, NULL, NULL)) {
    return;
  }

  upb_map_delete(intern->map, upb_key);
}

PHP_METHOD(MapField, count) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  RETURN_LONG(upb_map_size(intern->map));
}

PHP_METHOD(MapField, getIterator) {
  MapField *intern = (MapField*)Z_OBJ_P(getThis());
  zval ret;

  MapFieldIter_make(&ret, getThis());
  RETURN_ZVAL(&ret, 0, 1);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_offsetGet, 0, 0, 1)
  ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_offsetSet, 0, 0, 2)
  ZEND_ARG_INFO(0, index)
  ZEND_ARG_INFO(0, newval)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_void, 0)
ZEND_END_ARG_INFO()

static zend_function_entry map_field_methods[] = {
  PHP_ME(MapField, __construct,  NULL,              ZEND_ACC_PUBLIC)
  PHP_ME(MapField, offsetExists, arginfo_offsetGet, ZEND_ACC_PUBLIC)
  PHP_ME(MapField, offsetGet,    arginfo_offsetGet, ZEND_ACC_PUBLIC)
  PHP_ME(MapField, offsetSet,    arginfo_offsetSet, ZEND_ACC_PUBLIC)
  PHP_ME(MapField, offsetUnset,  arginfo_offsetGet, ZEND_ACC_PUBLIC)
  PHP_ME(MapField, count,        arginfo_void,      ZEND_ACC_PUBLIC)
  PHP_ME(MapField, getIterator,  arginfo_void,      ZEND_ACC_PUBLIC)
  ZEND_FE_END
};

static void map_field_init() {
  zend_class_entry tmp_ce;
  zend_object_handlers *h = &map_field_object_handlers;

  // TODO(haberman): see if we can mark this final, for robustness.
  INIT_CLASS_ENTRY(tmp_ce, "Google\\Protobuf\\Internal\\MapField",
                   map_field_methods);

  map_field_ce = zend_register_internal_class(&tmp_ce);
  zend_class_implements(map_field_ce, 3, spl_ce_ArrayAccess,
                        zend_ce_aggregate, spl_ce_Countable);
  map_field_ce->create_object = map_field_create;

  memcpy(h, &std_object_handlers, sizeof(zend_object_handlers));
  h->free_obj = map_field_free;
  h->get_gc = map_field_get_gc;
}

// -----------------------------------------------------------------------------
// MapFieldIter
// -----------------------------------------------------------------------------
//
typedef struct MapFieldIter {
  zend_object std;
  // TODO(haberman): does this need to be a zval so it's GC-rooted?
  MapField* map_field;
  size_t position;
} MapFieldIter;

zend_class_entry *map_field_iter_ce;
static zend_object_handlers map_field_iter_object_handlers;

/**
 * Object handler to create a MapFieldIter.
 */
zend_object* map_field_iter_create(zend_class_entry *class_type) {
  MapFieldIter *intern = emalloc(sizeof(MapFieldIter));
  zend_object_std_init(&intern->std, class_type);
  intern->std.handlers = &map_field_iter_object_handlers;
  intern->map_field = NULL;
  intern->position = 0;
  // Skip object_properties_init(), we don't allow derived classes.
  return &intern->std;
}

static void MapFieldIter_make(zval *val, zval *map_field) {
  MapFieldIter *iter;
  ZVAL_OBJ(val, map_field_iter_ce->create_object(map_field_iter_ce));
  iter = (MapFieldIter*)Z_OBJ_P(val);
  iter->map_field = (MapField*)Z_OBJ_P(map_field);
}

// -----------------------------------------------------------------------------
// PHP MapFieldIter Methods
// -----------------------------------------------------------------------------

PHP_METHOD(MapFieldIter, rewind) {
  MapFieldIter *intern = (MapFieldIter*)Z_OBJ_P(getThis());
  MapField *map_field = intern->map_field;
  intern->position = UPB_MAP_BEGIN;
  upb_mapiter_next(map_field->map, &intern->position);
}

PHP_METHOD(MapFieldIter, current) {
  MapFieldIter *intern = (MapFieldIter*)Z_OBJ_P(getThis());
  MapField *map_field = intern->map_field;
  upb_msgval upb_val = upb_mapiter_value(map_field->map, intern->position);
  zval ret;
  pbphp_tozval(upb_val, &ret, map_field->value_type, map_field->klass);
  RETURN_ZVAL(&ret, 0, 1);
}

PHP_METHOD(MapFieldIter, key) {
  MapFieldIter *intern = (MapFieldIter*)Z_OBJ_P(getThis());
  MapField *map_field = intern->map_field;
  upb_msgval upb_key = upb_mapiter_key(map_field->map, intern->position);
  zval ret;
  pbphp_tozval(upb_key, &ret, map_field->value_type, NULL);
  RETURN_ZVAL(&ret, 0, 1);
}

PHP_METHOD(MapFieldIter, next) {
  MapFieldIter *intern = (MapFieldIter*)Z_OBJ_P(getThis());
  upb_mapiter_next(intern->map_field->map, &intern->position);
}

PHP_METHOD(MapFieldIter, valid) {
  MapFieldIter *intern = (MapFieldIter*)Z_OBJ_P(getThis());
  bool done = upb_mapiter_done(intern->map_field->map, intern->position);
  RETURN_BOOL(!done);
}

static zend_function_entry map_field_iter_methods[] = {
  PHP_ME(MapFieldIter, rewind,      arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, current,     arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, key,         arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, next,        arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, valid,       arginfo_void, ZEND_ACC_PUBLIC)
  ZEND_FE_END
};

static void map_field_iter_init() {
  zend_class_entry tmp_ce;
  zend_object_handlers *h = &map_field_iter_object_handlers;

  // TODO(haberman): see if we can mark this final, for robustness.
  INIT_CLASS_ENTRY(tmp_ce, "Google\\Protobuf\\Internal\\MapFieldIter",
                   map_field_iter_methods);

  map_field_iter_ce = zend_register_internal_class(&tmp_ce);
  zend_class_implements(map_field_iter_ce, 1, zend_ce_iterator);
  map_field_iter_ce->create_object = map_field_iter_create;

  memcpy(h, &std_object_handlers, sizeof(zend_object_handlers));
}

void map_module_init() {
  map_field_init();
  map_field_iter_init();
}

#if 0

// Utilities

void* upb_value_memory(upb_value* v) {
  return (void*)(&v->val);
}

// -----------------------------------------------------------------------------
// Basic map operations on top of upb's strtable.
//
// Note that we roll our own `Map` container here because, as for
// `RepeatedField`, we want a strongly-typed container. This is so that any user
// errors due to incorrect map key or value types are raised as close as
// possible to the error site, rather than at some deferred point (e.g.,
// serialization).
//
// We build our `Map` on top of upb_strtable so that we're able to take
// advantage of the native_slot storage abstraction, as RepeatedField does.
// (This is not quite a perfect mapping -- see the key conversions below -- but
// gives us full support and error-checking for all value types for free.)
// -----------------------------------------------------------------------------

// Map values are stored using the native_slot abstraction (as with repeated
// field values), but keys are a bit special. Since we use a strtable, we need
// to store keys as sequences of bytes such that equality of those bytes maps
// one-to-one to equality of keys. We store strings directly (i.e., they map to
// their own bytes) and integers as native integers (using the native_slot
// abstraction).

// Note that there is another tradeoff here in keeping string keys as native
// strings rather than PHP strings: traversing the Map requires conversion to
// PHP string values on every traversal, potentially creating more garbage. We
// should consider ways to cache a PHP version of the key if this becomes an
// issue later.

// Forms a key to use with the underlying strtable from a PHP key value. |buf|
// must point to TABLE_KEY_BUF_LENGTH bytes of temporary space, used to
// construct a key byte sequence if needed. |out_key| and |out_length| provide
// the resulting key data/length.
#define TABLE_KEY_BUF_LENGTH 8  // sizeof(uint64_t)
static bool table_key(Map* self, zval* key,
                      char* buf,
                      const char** out_key,
                      size_t* out_length TSRMLS_DC) {
  switch (self->key_type) {
    case UPB_TYPE_STRING:
      if (!protobuf_convert_to_string(key)) {
        return false;
      }
      if (!is_structurally_valid_utf8(Z_STRVAL_P(key), Z_STRLEN_P(key))) {
        zend_error(E_USER_ERROR, "Given key is not UTF8 encoded.");
        return false;
      }
      *out_key = Z_STRVAL_P(key);
      *out_length = Z_STRLEN_P(key);
      break;

#define CASE_TYPE(upb_type, type, c_type, php_type)                     \
  case UPB_TYPE_##upb_type: {                                           \
    c_type type##_value;                                                \
    if (!protobuf_convert_to_##type(key, &type##_value)) {              \
      return false;                                                     \
    }                                                                   \
    native_slot_set_by_array(self->key_type, NULL, buf, key TSRMLS_CC); \
    *out_key = buf;                                                     \
    *out_length = native_slot_size(self->key_type);                     \
    break;                                                              \
  }
      CASE_TYPE(BOOL, bool, int8_t, BOOL)
      CASE_TYPE(INT32, int32, int32_t, LONG)
      CASE_TYPE(INT64, int64, int64_t, LONG)
      CASE_TYPE(UINT32, uint32, uint32_t, LONG)
      CASE_TYPE(UINT64, uint64, uint64_t, LONG)

#undef CASE_TYPE

    default:
      // Map constructor should not allow a Map with another key type to be
      // constructed.
      assert(false);
      break;
  }

  return true;
}

// -----------------------------------------------------------------------------
// MapField methods
// -----------------------------------------------------------------------------

// Forward declare static functions.

static void map_field_write_dimension(zval *object, zval *key,
                                      zval *value TSRMLS_DC);

// -----------------------------------------------------------------------------
// MapField creation/destruction
// -----------------------------------------------------------------------------

zend_class_entry* map_field_type;
zend_class_entry* map_field_iter_type;

zend_object_handlers* map_field_handlers;
zend_object_handlers* map_field_iter_handlers;

static void map_begin_internal(Map *map, MapIter *iter) {
  iter->self = map;
  upb_strtable_begin(&iter->it, &map->table);
}

static HashTable *map_field_get_gc(zval *object, CACHED_VALUE **table,
                                   int *n TSRMLS_DC) {
  // TODO(teboring): Unfortunately, zend engine does not support garbage
  // collection for custom array. We have to use zend engine's native array
  // instead.
  *table = NULL;
  *n = 0;
  return NULL;
}

// Define map value element free function.
static inline void php_proto_map_string_release(void *value) {
  zend_string* object = *(zend_string**)value;
  zend_string_release(object);
}
static inline void php_proto_map_object_release(void *value) {
  zend_object* object = *(zend_object**)value;
  GC_DELREF(object);
  if(GC_REFCOUNT(object) == 0) {
    zend_objects_store_del(object);
  }
}

// Define object free method.
PHP_PROTO_OBJECT_FREE_START(Map, map_field)
MapIter it;
int len;
for (map_begin_internal(intern, &it); !map_done(&it); map_next(&it)) {
  upb_value value = map_iter_value(&it, &len);
  void *mem = upb_value_memory(&value);
  switch (intern->value_type) {
    case UPB_TYPE_MESSAGE:
      php_proto_map_object_release(mem);
      break;
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES:
      php_proto_map_string_release(mem);
      break;
    default:
      break;
  }
}
upb_strtable_uninit(&intern->table);
PHP_PROTO_OBJECT_FREE_END

PHP_PROTO_OBJECT_EMPTY_DTOR_START(Map, map_field)
PHP_PROTO_OBJECT_DTOR_END

// Define object create method.
PHP_PROTO_OBJECT_CREATE_START(Map, map_field)
// Table value type is always UINT64: this ensures enough space to store the
// native_slot value.
if (!upb_strtable_init(&intern->table, UPB_CTYPE_UINT64)) {
  zend_error(E_USER_ERROR, "Could not allocate table.");
}
PHP_PROTO_OBJECT_CREATE_END(Map, map_field)

// Init class entry.
PHP_PROTO_INIT_CLASS_START("Google\\Protobuf\\Internal\\MapField", Map,
                           map_field)
zend_class_implements(map_field_type TSRMLS_CC, 3, spl_ce_ArrayAccess,
                      zend_ce_aggregate, spl_ce_Countable);
map_field_handlers->write_dimension = map_field_write_dimension;
map_field_handlers->get_gc = map_field_get_gc;
PHP_PROTO_INIT_CLASS_END

void map_field_ensure_created(const upb_fielddef *field,
                              CACHED_VALUE *map_field PHP_PROTO_TSRMLS_DC) {
  if (ZVAL_IS_NULL(CACHED_PTR_TO_ZVAL_PTR(map_field))) {
    zval_ptr_dtor(map_field);
    map_field_create_with_field(map_field_type, field,
                                map_field PHP_PROTO_TSRMLS_CC);
  }
}

void map_field_create_with_field(const zend_class_entry *ce,
                                 const upb_fielddef *field,
                                 CACHED_VALUE *map_field PHP_PROTO_TSRMLS_DC) {
  const upb_fielddef *key_field = map_field_key(field);
  const upb_fielddef *value_field = map_field_value(field);
  map_field_create_with_type(
      ce, upb_fielddef_type(key_field), upb_fielddef_type(value_field),
      field_type_class(value_field TSRMLS_CC), map_field PHP_PROTO_TSRMLS_CC);
}

void map_field_create_with_type(const zend_class_entry *ce,
                                upb_fieldtype_t key_type,
                                upb_fieldtype_t value_type,
                                const zend_class_entry *msg_ce,
                                CACHED_VALUE *map_field PHP_PROTO_TSRMLS_DC) {
  CREATE_OBJ_ON_ALLOCATED_ZVAL_PTR(CACHED_PTR_TO_ZVAL_PTR(map_field),
                                   map_field_type);
  Map *intern = UNBOX(Map, CACHED_TO_ZVAL_PTR(*map_field));
  intern->key_type = key_type;
  intern->value_type = value_type;
  intern->msg_ce = msg_ce;
}

// -----------------------------------------------------------------------------
// MapField Handlers
// -----------------------------------------------------------------------------

static bool map_field_read_dimension(zval *object, zval *key, int type,
                                     CACHED_VALUE *retval TSRMLS_DC) {
  Map *intern = UNBOX(Map, object);

  char keybuf[TABLE_KEY_BUF_LENGTH];
  const char* keyval = NULL;
  size_t length = 0;
  upb_value v;
#ifndef NDEBUG
  v.ctype = UPB_CTYPE_UINT64;
#endif
  if (!table_key(intern, key, keybuf, &keyval, &length TSRMLS_CC)) {
    return false;
  }

  if (upb_strtable_lookup2(&intern->table, keyval, length, &v)) {
    void* mem = upb_value_memory(&v);
    native_slot_get_by_map_value(intern->value_type, mem, retval TSRMLS_CC);
    return true;
  } else {
    zend_error(E_USER_ERROR, "Given key doesn't exist.");
    return false;
  }
}

static void map_index_unset(Map *intern, const char* keyval, int length) {
  upb_value old_value;
  if (upb_strtable_remove2(&intern->table, keyval, length, &old_value)) {
    switch (intern->value_type) {
      case UPB_TYPE_MESSAGE: {
        zend_object* object = *(zend_object**)upb_value_memory(&old_value);
        GC_DELREF(object);
        if(GC_REFCOUNT(object) == 0) {
          zend_objects_store_del(object);
        }
        break;
      }
      case UPB_TYPE_STRING:
      case UPB_TYPE_BYTES: {
        zend_string* object = *(zend_string**)upb_value_memory(&old_value);
        zend_string_release(object);
        break;
      }
      default:
        break;
    }
  }
}

bool map_index_set(Map *intern, const char* keyval, int length, upb_value v) {
  // Replace any existing value by issuing a 'remove' operation first.
  map_index_unset(intern, keyval, length);

  if (!upb_strtable_insert2(&intern->table, keyval, length, v)) {
    zend_error(E_USER_ERROR, "Could not insert into table");
    return false;
  }

  return true;
}

static void map_field_write_dimension(zval *object, zval *key,
                                      zval *value TSRMLS_DC) {
  Map *intern = UNBOX(Map, object);

  char keybuf[TABLE_KEY_BUF_LENGTH];
  const char* keyval = NULL;
  size_t length = 0;
  upb_value v;
  void* mem;
  if (!table_key(intern, key, keybuf, &keyval, &length TSRMLS_CC)) {
    return;
  }

  mem = upb_value_memory(&v);
  memset(mem, 0, native_slot_size(intern->value_type));
  if (!native_slot_set_by_map(intern->value_type, intern->msg_ce, mem,
                                value TSRMLS_CC)) {
    return;
  }
#ifndef NDEBUG
  v.ctype = UPB_CTYPE_UINT64;
#endif

  map_index_set(intern, keyval, length, v);
}

static bool map_field_unset_dimension(zval *object, zval *key TSRMLS_DC) {
  Map *intern = UNBOX(Map, object);

  char keybuf[TABLE_KEY_BUF_LENGTH];
  const char* keyval = NULL;
  size_t length = 0;
  if (!table_key(intern, key, keybuf, &keyval, &length TSRMLS_CC)) {
    return false;
  }
#ifndef NDEBUG
  v.ctype = UPB_CTYPE_UINT64;
#endif

  map_index_unset(intern, keyval, length);

  return true;
}

// -----------------------------------------------------------------------------
// PHP MapField Methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Map Iterator
// -----------------------------------------------------------------------------

void map_begin(zval *map_php, MapIter *iter TSRMLS_DC) {
  Map *self = UNBOX(Map, map_php);
  map_begin_internal(self, iter);
}

void map_next(MapIter *iter) {
  upb_strtable_next(&iter->it);
}

bool map_done(MapIter *iter) {
  return upb_strtable_done(&iter->it);
}

const char *map_iter_key(MapIter *iter, int *len) {
  *len = upb_strtable_iter_keylength(&iter->it);
  return upb_strtable_iter_key(&iter->it);
}

upb_value map_iter_value(MapIter *iter, int *len) {
  *len = native_slot_size(iter->self->value_type);
  return upb_strtable_iter_value(&iter->it);
}

// -----------------------------------------------------------------------------
// MapFieldIter methods
// -----------------------------------------------------------------------------
static zend_function_entry map_field_iter_methods[] = {
  PHP_ME(MapFieldIter, rewind,      arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, current,     arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, key,         arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, next,        arginfo_void, ZEND_ACC_PUBLIC)
  PHP_ME(MapFieldIter, valid,       arginfo_void, ZEND_ACC_PUBLIC)
  ZEND_FE_END
};

// -----------------------------------------------------------------------------
// MapFieldIter creation/destruction
// -----------------------------------------------------------------------------

// Define object free method.
PHP_PROTO_OBJECT_EMPTY_FREE_START(MapIter, map_field_iter)
PHP_PROTO_OBJECT_FREE_END

PHP_PROTO_OBJECT_EMPTY_DTOR_START(MapIter, map_field_iter)
PHP_PROTO_OBJECT_DTOR_END

// Define object create method.
PHP_PROTO_OBJECT_CREATE_START(MapIter, map_field_iter)
intern->self = NULL;
PHP_PROTO_OBJECT_CREATE_END(MapIter, map_field_iter)

// Init class entry.
PHP_PROTO_INIT_CLASS_START("Google\\Protobuf\\Internal\\MapFieldIter",
                           MapIter, map_field_iter)
zend_class_implements(map_field_iter_type TSRMLS_CC, 1, zend_ce_iterator);
PHP_PROTO_INIT_CLASS_END

// -----------------------------------------------------------------------------
// PHP MapFieldIter Methods
// -----------------------------------------------------------------------------

PHP_METHOD(MapFieldIter, rewind) {
  MapIter *intern = UNBOX(MapIter, getThis());
  map_begin_internal(intern->self, intern);
}

PHP_METHOD(MapFieldIter, current) {
  MapIter *intern = UNBOX(MapIter, getThis());
  Map *map_field = intern->self;

  int value_length = 0;
  upb_value value = map_iter_value(intern, &value_length);

  void* mem = upb_value_memory(&value);
  native_slot_get_by_map_value(map_field->value_type, mem,
                               ZVAL_PTR_TO_CACHED_PTR(return_value) TSRMLS_CC);
}

PHP_METHOD(MapFieldIter, key) {
  MapIter *intern = UNBOX(MapIter, getThis());
  Map *map_field = intern->self;

  int key_length = 0;
  const char* key = map_iter_key(intern, &key_length);

  native_slot_get_by_map_key(map_field->key_type, key, key_length,
                             ZVAL_PTR_TO_CACHED_PTR(return_value) TSRMLS_CC);
}

PHP_METHOD(MapFieldIter, next) {
  MapIter *intern = UNBOX(MapIter, getThis());
  map_next(intern);
}

PHP_METHOD(MapFieldIter, valid) {
  MapIter *intern = UNBOX(MapIter, getThis());
  RETURN_BOOL(!map_done(intern));
}
#endif
