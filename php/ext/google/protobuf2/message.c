// Protocol Buffers - Google's data interchange format
// Copyright 2014 Google Inc.  All rights reserved.
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

#include <inttypes.h>
#include <php.h>
#include <Zend/zend_exceptions.h>
#include <stdlib.h>

#include "arena.h"
#include "array.h"
#include "convert.h"
#include "def.h"
#include "map.h"
#include "message.h"
#include "php-upb.h"
#include "protobuf.h"

// -----------------------------------------------------------------------------
// Message
// -----------------------------------------------------------------------------

typedef struct {
  zend_object std;
  zval arena;
  const Descriptor* desc;
  upb_msg *msg;
} Message;

zend_class_entry *message_ce;
static zend_object_handlers message_object_handlers;

/**
 * Object handler to create a Message.
 */
static zend_object* message_create(zend_class_entry *class_type) {
  Message *intern = emalloc(sizeof(Message));
  // XXX(haberman): verify whether we actually want to take this route.
  class_type->default_properties_count = 0;
  zend_object_std_init(&intern->std, class_type);
  intern->std.handlers = &message_object_handlers;
  arena_init(&intern->arena);
  return &intern->std;
}

void pbphp_getmsg(zval *val, const Descriptor *desc, upb_msg *msg,
                  zval *arena) {
  if (!msg) {
    ZVAL_NULL(val);
    return;
  }

  if (!pbphp_cacheget(msg, val)) {
    Message *intern = emalloc(sizeof(Message));
    // XXX(haberman): verify whether we actually want to take this route.
    desc->class_entry->default_properties_count = 0;
    zend_object_std_init(&intern->std, desc->class_entry);
    intern->std.handlers = &message_object_handlers;
    ZVAL_COPY(&intern->arena, arena);
    intern->desc = desc;
    intern->msg = msg;
    ZVAL_OBJ(val, &intern->std);
    pbphp_cacheadd(intern->msg, &intern->std);
  }
}

bool pbphp_tomsg(zval *val, const Descriptor *desc, upb_arena *arena, upb_msg **msg) {
  PBPHP_ASSERT(desc);

  if (Z_ISREF_P(val)) {
    ZVAL_DEREF(val);
  }

  if (Z_TYPE_P(val) == IS_NULL) {
    *msg = NULL;
    return true;
  }

  if (Z_TYPE_P(val) == IS_OBJECT &&
      instanceof_function(Z_OBJCE_P(val), desc->class_entry)) {
    Message *intern = (Message*)Z_OBJ_P(val);
    upb_arena_fuse(arena, arena_get(&intern->arena));
    *msg = intern->msg;
    return true;
  } else {
    php_error_docref(NULL, E_USER_ERROR,
                     "Given value is not an instance of %s.",
                     ZSTR_VAL(desc->class_entry->name));
    return false;
  }
}

/**
 * Object handler to free a Message.
 */
static void message_dtor(zend_object* obj) {
  Message* intern = (Message*)obj;
  pbphp_cachedel(intern->msg);
  zval_dtor(&intern->arena);
  zend_object_std_dtor(&intern->std);
}

static const upb_fielddef *get_field(Message *msg, zval *member) {
  const upb_msgdef *m = msg->desc->msgdef;
  const upb_fielddef *f =
      upb_msgdef_ntof(m, Z_STRVAL_P(member), Z_STRLEN_P(member));

  if (!f) {
    php_error_docref(NULL, E_USER_ERROR,
                     "No such property %s.",
                     ZSTR_VAL(msg->desc->class_entry->name));
  }

  return f;
}

static zval *message_read_property(zval *obj, zval *member, int type,
                                   void **cache_slot, zval *rv) {
  Message* intern = (Message*)Z_OBJ_P(obj);
  const upb_fielddef *f = get_field(intern, member);
  upb_arena *arena = arena_get(&intern->arena);
  upb_msgval msgval;

  if (!f) return NULL;


  if (upb_fielddef_ismap(f)) {
    upb_mutmsgval msgval = upb_msg_mutable(intern->msg, f, arena);
    pbphp_getmapfield(rv, msgval.map, f, &intern->arena);
  } else if (upb_fielddef_isseq(f)) {
    upb_mutmsgval msgval = upb_msg_mutable(intern->msg, f, arena);
    pbphp_getrepeatedfield(rv, msgval.array, f, &intern->arena);
  } else {
    upb_msgval msgval = upb_msg_get(intern->msg, f);
    upb_fieldtype_t type = upb_fielddef_type(f);
    const Descriptor *subdesc =
        pupb_getdesc_from_msgdef(upb_fielddef_msgsubdef(f));
    pbphp_tozval(msgval, rv, upb_fielddef_type(f), subdesc, &intern->arena);
  }

  return rv;
}

static void pbphp_msg_set(Message *intern, const upb_fielddef *f, zval *val) {
  upb_arena *arena = arena_get(&intern->arena);
  upb_msgval msgval;

  if (upb_fielddef_ismap(f)) {
    msgval.map_val = pbphp_getmap(val, f, arena);
    if (!msgval.map_val) return;
  } else if (upb_fielddef_isseq(f)) {
    msgval.array_val = pbphp_getarr(val, f, arena);
    if (!msgval.array_val) return;
  } else {
    upb_fieldtype_t type = upb_fielddef_type(f);
    const Descriptor *subdesc =
        pupb_getdesc_from_msgdef(upb_fielddef_msgsubdef(f));
    bool ok = pbphp_tomsgval(val, &msgval, type, subdesc, arena);
    if (!ok) return;
  }

  upb_msg_set(intern->msg, f, msgval, arena);
}

static void message_write_property(zval *obj, zval *member, zval *val,
                                   void **cache_slot) {
  Message* intern = (Message*)Z_OBJ_P(obj);
  const upb_fielddef *f = get_field(intern, member);

  if (!f) return;

  pbphp_msg_set(intern, f, val);
}

static zval* message_get_property_ptr_ptr(zval* object, zval* member, int type,
                                          void** cache_slot) {
  return NULL;
}

static HashTable* message_get_properties(zval* object TSRMLS_DC) {
  return NULL;
}

static bool init_msg(upb_msg *msg, const upb_msgdef *m, zval *init,
                     upb_arena *arena) {
  HashTable* table = HASH_OF(init);
  HashPosition pos;

  if (Z_ISREF_P(init)) {
    ZVAL_DEREF(init);
  }

  if (Z_TYPE_P(init) != IS_ARRAY) {
    php_error_docref(NULL, E_USER_ERROR,
                     "Initializer for a message %s must be an array.",
                     upb_msgdef_fullname(m));
    return false;
  }

  zend_hash_internal_pointer_reset_ex(table, &pos);
  while (true) {
    zval key;
    zval *val;
    const upb_fielddef *f;

    zend_hash_get_current_key_zval_ex(table, &key, &pos);
    val = zend_hash_get_current_data_ex(table, &pos);

    if (!val) break;

    f = upb_msgdef_ntof(m, Z_STRVAL_P(&key), Z_STRLEN_P(&key));

    if (!f) return false;

    if (upb_fielddef_ismap(f)) {
      upb_mutmsgval msgval = upb_msg_mutable(msg, f, arena);
      if (!pbphp_map_init(msgval.map, f, val, arena)) return false;
    } else if (upb_fielddef_isseq(f)) {
      upb_mutmsgval msgval = upb_msg_mutable(msg, f, arena);
      if (!pbphp_array_init(msgval.array, f, val, arena)) return false;
    } else {
      // By handling submessages in this case, we only allow:
      //   ['foo_submsg': new Foo(['a' => 1])]
      // not:
      //   ['foo_submsg': ['a' => 1]]
      upb_fieldtype_t type = upb_fielddef_type(f);
      const Descriptor *desc =
          pupb_getdesc_from_msgdef(upb_fielddef_msgsubdef(f));
      upb_msgval msgval;
      if (!pbphp_tomsgval(val, &msgval, type, desc, arena)) return false;
      upb_msg_set(msg, f, msgval, arena);
    }

    zend_hash_move_forward_ex(table, &pos);
    zval_dtor(&key);
  }
}

PHP_METHOD(Message, __construct) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  const Descriptor* desc = pupb_getdesc(Z_OBJCE_P(getThis()));
  const upb_msgdef *msgdef = desc->msgdef;
  upb_arena *arena = arena_get(&intern->arena);
  zval *init_arr;

  intern->desc = desc;
  intern->msg = upb_msg_new(msgdef, arena);
  pbphp_cacheadd(intern->msg, &intern->std);

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!", &init_arr) == FAILURE) {
    return;
  }

  if (init_arr) {
    init_msg(intern->msg, desc->msgdef, init_arr, arena);
  }
}

PHP_METHOD(Message, clear) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  upb_msg_clear(intern->msg, intern->desc->msgdef);
}

PHP_METHOD(Message, mergeFrom) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  Message* from;
  upb_arena *arena = arena_get(&intern->arena);
  const upb_msglayout *l = upb_msgdef_layout(intern->desc->msgdef);
  zval* value;
  char *pb;
  size_t size;
  bool ok;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &value,
                            intern->desc->class_entry) == FAILURE) {
    return;
  }

  from = (Message*)Z_OBJ_P(value);

  // Should be guaranteed since we passed the class type to
  // zend_parse_parameters().
  PBPHP_ASSERT(from->desc == intern->desc);

  // TODO(haberman): use a temp arena for this once we can make upb_decode()
  // copy strings.
  pb = upb_encode(from->msg, l, arena, &size);

  if (!pb) {
    php_error_docref(NULL, E_USER_ERROR, "Max nesting exceeded");
    return;
  }

  ok = upb_decode(pb, size, intern->msg, l, arena);
  PBPHP_ASSERT(ok);
}

PHP_METHOD(Message, mergeFromString) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  char *data = NULL;
  zend_long data_len;
  const upb_msglayout *l = upb_msgdef_layout(intern->desc->msgdef);
  upb_arena *arena = arena_get(&intern->arena);

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &data, &data_len) ==
      FAILURE) {
    return;
  }

  if (!upb_decode(data, data_len, intern->msg, l, arena)) {
    zend_throw_exception_ex(NULL, 0, "Error occurred during parsing");
    return;
  }
}

PHP_METHOD(Message, serializeToString) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  const upb_msglayout *l = upb_msgdef_layout(intern->desc->msgdef);
  upb_arena *tmp_arena = upb_arena_new();
  char *data;
  size_t size;

  data = upb_encode(intern->msg, l, tmp_arena, &size);

  if (!data) {
    zend_throw_exception_ex(NULL, 0, "Error occurred during serialization");
    return;
  }

  RETURN_STRINGL(data, size);
  upb_arena_free(tmp_arena);
}

PHP_METHOD(Message, mergeFromJsonString) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  char *data = NULL;
  zend_long data_len;
  const upb_msglayout *l = upb_msgdef_layout(intern->desc->msgdef);
  upb_arena *arena = arena_get(&intern->arena);
  upb_status status;
  zend_bool ignore_json_unknown = false;
  int options = 0;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|b", &data, &data_len,
                            &ignore_json_unknown) == FAILURE) {
    return;
  }

  if (ignore_json_unknown) {
    options |= UPB_JSONDEC_IGNOREUNKNOWN;
  }

  upb_status_clear(&status);
  if (!upb_json_decode(data, data_len, intern->msg, intern->desc->msgdef,
                       descriptor_pool_getsymtab(), 0, arena, &status)) {
    zend_throw_exception_ex(NULL, 0, "Error occurred during parsing",
                            upb_status_errmsg(&status));
    return;
  }
}

PHP_METHOD(Message, serializeToJsonString) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  const upb_msglayout *l = upb_msgdef_layout(intern->desc->msgdef);
  upb_arena *tmp_arena = upb_arena_new();
  char *data;
  size_t size;
  int options = 0;
  char buf[1024];
  zend_bool preserve_proto_fieldnames = false;
  upb_status status;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b",
                            &preserve_proto_fieldnames) == FAILURE) {
    return;
  }

  if (preserve_proto_fieldnames) {
    options |= UPB_JSONENC_PROTONAMES;
  }

  upb_status_clear(&status);
  size = upb_json_encode(intern->msg, intern->desc->msgdef,
                         descriptor_pool_getsymtab(), options, buf, sizeof(buf),
                         &status);

  if (!upb_ok(&status)) {
    zend_throw_exception_ex(NULL, 0, "Error occurred during serialization");
    return;
  }

  if (size >= sizeof(buf)) {
    char *buf2 = malloc(size + 1);
    upb_json_encode(intern->msg, intern->desc->msgdef,
                    descriptor_pool_getsymtab(), options, buf2, size + 1,
                    &status);
    RETURN_STRINGL(buf2, size);
    free(buf2);
  } else {
    RETURN_STRINGL(buf, size);
  }
}

PHP_METHOD(Message, readWrapperValue) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  char* member;
  const upb_fielddef *f;
  zend_long size;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &member, &size) == FAILURE) {
    return;
  }

  f = upb_msgdef_ntof(intern->desc->msgdef, member, size);
  PBPHP_ASSERT(f);

  if (upb_msg_has(intern->msg, f)) {
    const upb_msg *wrapper = upb_msg_get(intern->msg, f).msg_val;
    const upb_msgdef *m = upb_fielddef_msgsubdef(f);
    const upb_fielddef *val_f = upb_msgdef_itof(m, 1);
    upb_msgval msgval = upb_msg_get(wrapper, val_f);
    zval ret;
    pbphp_tozval(msgval, &ret, upb_fielddef_type(val_f), NULL, &intern->arena);
    RETURN_ZVAL(&ret, 1, 0);
  } else {
    RETURN_NULL();
  }
}

PHP_METHOD(Message, writeWrapperValue) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  upb_arena *arena = arena_get(&intern->arena);
  char* member;
  const upb_fielddef *f;
  upb_msgval msgval;
  zend_long size;
  zval* val;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz", &member, &size, &val) ==
      FAILURE) {
    return;
  }

  f = upb_msgdef_ntof(intern->desc->msgdef, member, size);
  PBPHP_ASSERT(f);

  if (pbphp_tomsgval(val, &msgval, upb_fielddef_type(f),
                     pupb_getdesc_from_msgdef(upb_fielddef_msgsubdef(f)),
                     arena)) {
    upb_msg *wrapper = upb_msg_mutable(intern->msg, f, arena).msg;
    const upb_msgdef *m = upb_fielddef_msgsubdef(f);
    const upb_fielddef *val_f = upb_msgdef_itof(m, 1);
    upb_msg_set(wrapper, val_f, msgval, arena);
  }
}

PHP_METHOD(Message, whichOneof) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  const upb_oneofdef* oneof;
  const upb_fielddef* field;
  char* name;
  zend_long len;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &len) == FAILURE) {
    return;
  }

  oneof = upb_msgdef_ntoo(intern->desc->msgdef, name, len);

  if (!oneof) {
    php_error_docref(NULL, E_USER_ERROR, "Message %s has no oneof %s.",
                     upb_msgdef_fullname(intern->desc->msgdef), name);
    return;
  }

  field = upb_msg_whichoneof(intern->msg, oneof);
  RETURN_STRING(field ? upb_fielddef_name(field) : "");
}

PHP_METHOD(Message, readOneof) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  zend_long field_num;
  const upb_fielddef* f;
  zval ret;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &field_num) == FAILURE) {
    return;
  }

  f = upb_msgdef_itof(intern->desc->msgdef, field_num);

  if (!f) {
    php_error_docref(NULL, E_USER_ERROR,
                     "Internal error, no such oneof field %d\n",
                     (int)field_num);
  }

  {
    upb_msgval msgval = upb_msg_get(intern->msg, f);
    const Descriptor *subdesc =
        pupb_getdesc_from_msgdef(upb_fielddef_msgsubdef(f));
    pbphp_tozval(msgval, &ret, upb_fielddef_type(f), subdesc, &intern->arena);
  }

  RETURN_ZVAL(&ret, 1, 0);
}

PHP_METHOD(Message, writeOneof) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  zend_long field_num;
  const upb_fielddef* f;
  upb_arena *arena = arena_get(&intern->arena);
  upb_msgval msgval;
  zval* val;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &field_num, &val) ==
      FAILURE) {
    return;
  }

  f = upb_msgdef_itof(intern->desc->msgdef, field_num);

  if (!pbphp_tomsgval(val, &msgval, upb_fielddef_type(f),
                      pupb_getdesc_from_msgdef(upb_fielddef_msgsubdef(f)),
                      arena)) {
    return;
  }

  upb_msg_set(intern->msg, f, msgval, arena);
}


static  zend_function_entry message_methods[] = {
  PHP_ME(Message, clear, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, discardUnknownFields, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(Message, serializeToString, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(Message, mergeFromString, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(Message, serializeToJsonString, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(Message, mergeFromJsonString, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(Message, mergeFrom, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(Message, readWrapperValue, NULL, ZEND_ACC_PROTECTED)
  PHP_ME(Message, writeWrapperValue, NULL, ZEND_ACC_PROTECTED)
  PHP_ME(Message, readOneof, NULL, ZEND_ACC_PROTECTED)
  PHP_ME(Message, writeOneof, NULL, ZEND_ACC_PROTECTED)
  PHP_ME(Message, whichOneof, NULL, ZEND_ACC_PROTECTED)
  PHP_ME(Message, __construct, NULL, ZEND_ACC_PROTECTED)
  ZEND_FE_END
};

static void message_init() {
  zend_class_entry tmp_ce;
  zend_object_handlers *h = &message_object_handlers;

  // TODO(haberman): see if we can mark this final, for robustness.
  INIT_CLASS_ENTRY(tmp_ce, "Google\\Protobuf\\Internal\\Message",
                   message_methods);

  message_ce = zend_register_internal_class(&tmp_ce);
  message_ce->create_object = message_create;

  memcpy(h, &std_object_handlers, sizeof(zend_object_handlers));
  h->dtor_obj = message_dtor;
  h->read_property = message_read_property;
  h->write_property = message_write_property;
  h->get_properties = message_get_properties;
  h->get_property_ptr_ptr = message_get_property_ptr_ptr;
}

void message_module_init() {
  message_init();
}

#if 0

// -----------------------------------------------------------------------------
// C Message Utilities
// -----------------------------------------------------------------------------

void* message_data(MessageHeader* msg) {
  return msg->data;
}

void custom_data_init(const zend_class_entry* ce,
                      MessageHeader* intern PHP_PROTO_TSRMLS_DC) {
  DescriptorInternal* desc = get_ce_desc(ce);
  intern->data = ALLOC_N(uint8_t, desc->layout->size);
  // We wrap first so that everything in the message object is GC-rooted in
  // case a collection happens during object creation in layout_init().
  intern->descriptor = desc;
  layout_init(desc->layout, message_data(intern),
              &intern->std PHP_PROTO_TSRMLS_CC);
}

#define INIT_MESSAGE_WITH_ARRAY                                    \
  {                                                                \
    zval* array_wrapper = NULL;                                    \
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,           \
                              "|a!", &array_wrapper) == FAILURE) { \
      return;                                                      \
    }                                                              \
    Message_construct(getThis(), array_wrapper);                   \
  }

PHP_METHOD(Message, mergeFrom) {
  zval* value;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &value,
                            message_type) == FAILURE) {
    return;
  }

  MessageHeader* from = UNBOX(MessageHeader, value);
  MessageHeader* to = UNBOX(MessageHeader, getThis());

  if(from->descriptor != to->descriptor) {
    zend_error(E_USER_ERROR, "Cannot merge messages with different class.");
    return;
  }

  layout_merge(from->descriptor->layout, from, to TSRMLS_CC);
}

PHP_METHOD(Message, readWrapperValue) {
  char* member;
  PHP_PROTO_SIZE length;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &member,
                            &length) == FAILURE) {
    return;
  }

  MessageHeader* msg = UNBOX(MessageHeader, getThis());
  const upb_fielddef* field =
      upb_msgdef_ntofz(msg->descriptor->msgdef, member);

  if (upb_fielddef_containingoneof(field)) {
    uint32_t* oneof_case =
        slot_oneof_case(msg->descriptor->layout, message_data(msg), field);
    if (*oneof_case != upb_fielddef_number(field)) {
      RETURN_NULL();
    }
  }

  zval* cached_zval =
      CACHED_PTR_TO_ZVAL_PTR(find_zval_property(msg, field));

  if (Z_TYPE_P(cached_zval) == IS_NULL) {
    RETURN_NULL();
  }

  if (Z_TYPE_P(cached_zval) == IS_OBJECT) {
    const upb_msgdef* submsgdef = upb_fielddef_msgsubdef(field);
    const upb_fielddef* value_field = upb_msgdef_itof(submsgdef, 1);
    MessageHeader* submsg = UNBOX(MessageHeader, cached_zval);
    CACHED_VALUE* cached_value = find_zval_property(submsg, value_field);
    layout_get(submsg->descriptor->layout, submsg, value_field,
               cached_value TSRMLS_CC);
    RETURN_ZVAL(CACHED_PTR_TO_ZVAL_PTR(cached_value), 1, 0);
  } else {
    RETURN_ZVAL(cached_zval, 1, 0);
  }
}

PHP_METHOD(Message, writeWrapperValue) {
  char* member;
  PHP_PROTO_SIZE length;
  zval* value;
  if (zend_parse_parameters(
      ZEND_NUM_ARGS() TSRMLS_CC, "sz", &member, &length, &value) ==
      FAILURE) {
    return;
  }

  MessageHeader* msg = UNBOX(MessageHeader, getThis());
  const upb_fielddef* field = upb_msgdef_ntofz(msg->descriptor->msgdef, member);

  zval* cached_zval =
      CACHED_PTR_TO_ZVAL_PTR(find_zval_property(msg, field));

  if (Z_TYPE_P(value) == IS_NULL) {
    MessageHeader* msg = UNBOX(MessageHeader, getThis());
    layout_set(msg->descriptor->layout, msg,
               field, value TSRMLS_CC);
    return;
  }

  {
    // Type Checking
    const upb_msgdef* submsgdef = upb_fielddef_msgsubdef(field);
    const upb_fielddef* value_field = upb_msgdef_itof(submsgdef, 1);
    upb_fieldtype_t type = upb_fielddef_type(value_field);
    switch(type) {
      case UPB_TYPE_STRING:
      case UPB_TYPE_BYTES: {
        if (!protobuf_convert_to_string(value)) {
          return;
        }
        if (type == UPB_TYPE_STRING &&
            !is_structurally_valid_utf8(Z_STRVAL_P(value), Z_STRLEN_P(value))) {
          zend_error(E_USER_ERROR, "Given string is not UTF8 encoded.");
          return;
        }
      }
      break;
#define CASE_TYPE(upb_type, type, c_type)                    \
  case UPB_TYPE_##upb_type: {                                \
    c_type type##_value;                                     \
    if (!protobuf_convert_to_##type(value, &type##_value)) { \
      return;                                                \
    }                                                        \
    break;                                                   \
  }
      CASE_TYPE(INT32,  int32,  int32_t)
      CASE_TYPE(UINT32, uint32, uint32_t)
      CASE_TYPE(ENUM,   int32,  int32_t)
      CASE_TYPE(INT64,  int64,  int64_t)
      CASE_TYPE(UINT64, uint64, uint64_t)
      CASE_TYPE(FLOAT,  float,  float)
      CASE_TYPE(DOUBLE, double, double)
      CASE_TYPE(BOOL,   bool,   int8_t)

#undef CASE_TYPE
      case UPB_TYPE_MESSAGE:
        zend_error(E_ERROR, "No wrapper for message.");
        break;
    }
  }

  if (upb_fielddef_containingoneof(field)) {
    uint32_t* oneof_case =
        slot_oneof_case(msg->descriptor->layout, message_data(msg), field);
    if (*oneof_case != upb_fielddef_number(field)) {
      zval null_value;
      ZVAL_NULL(&null_value);
      layout_set(msg->descriptor->layout, msg, field, &null_value TSRMLS_CC);
      cached_zval = CACHED_PTR_TO_ZVAL_PTR(find_zval_property(msg, field));
      ZVAL_ZVAL(cached_zval, value, 1, 0);
      return;
    }
  }

  if (Z_TYPE_P(cached_zval) == IS_OBJECT) {
    const upb_msgdef* submsgdef = upb_fielddef_msgsubdef(field);
    const upb_fielddef* value_field = upb_msgdef_itof(submsgdef, 1);
    MessageHeader* submsg = UNBOX(MessageHeader, cached_zval);
    layout_set(submsg->descriptor->layout, submsg,
               value_field, value TSRMLS_CC);
  } else {
    ZVAL_ZVAL(cached_zval, value, 1, 0);
  }
}

PHP_METHOD(Message, readOneof) {
  PHP_PROTO_LONG index;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &index) ==
      FAILURE) {
    return;
  }

  MessageHeader* msg = UNBOX(MessageHeader, getThis());

  const upb_fielddef* field = upb_msgdef_itof(msg->descriptor->msgdef, index);

  // Unlike singular fields, oneof fields share cached property. So we cannot
  // let layout_get modify the cached property. Instead, we pass in the return
  // value directly.
  layout_get(msg->descriptor->layout, msg, field,
             ZVAL_PTR_TO_CACHED_PTR(return_value) TSRMLS_CC);
}

PHP_METHOD(Message, writeOneof) {
  PHP_PROTO_LONG index;
  zval* value;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &index, &value) ==
      FAILURE) {
    return;
  }

  MessageHeader* msg = UNBOX(MessageHeader, getThis());

  const upb_fielddef* field = upb_msgdef_itof(msg->descriptor->msgdef, index);

  layout_set(msg->descriptor->layout, msg, field, value TSRMLS_CC);
}

PHP_METHOD(Message, whichOneof) {
  char* oneof_name;
  PHP_PROTO_SIZE length;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &oneof_name,
                            &length) == FAILURE) {
    return;
  }

  MessageHeader* msg = UNBOX(MessageHeader, getThis());

  const upb_oneofdef* oneof =
      upb_msgdef_ntoo(msg->descriptor->msgdef, oneof_name, length);
  const char* oneof_case_name = layout_get_oneof_case(
      msg->descriptor->layout, message_data(msg), oneof TSRMLS_CC);
  RETURN_STRING(oneof_case_name);
}

// -----------------------------------------------------------------------------
#endif
