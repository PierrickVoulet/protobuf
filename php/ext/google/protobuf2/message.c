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

#include <Zend/zend_inheritance.h>
#include <inttypes.h>
#include <php.h>
#include <stdlib.h>

// This header does not include its dependencies.
#include <Zend/zend_exceptions.h>

#include "arena.h"
#include "convert.h"
#include "def.h"
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

void pbphp_msg_newwrapper(zval *val, const Descriptor *desc, upb_msg *msg,
                          zval *arena) {
  Message *intern = emalloc(sizeof(Message));
  zend_object_std_init(&intern->std, desc->class_entry);
  intern->std.handlers = &message_object_handlers;
  ZVAL_COPY(&intern->arena, arena);
  intern->desc = desc;
  intern->msg = msg;
  ZVAL_OBJ(val, &intern->std);
}

/**
 * Object handler to free a Message.
 */
static void message_dtor(zend_object* obj) {
  Message* intern = (Message*)obj;
  zval_dtor(&intern->arena);
  zend_object_std_dtor(&intern->std);
}

static const upb_fielddef *get_field(Message *msg, zval *member,
                                     const Descriptor **subdesc) {
  const upb_msgdef *m = msg->desc->msgdef;
  const upb_fielddef *f =
      upb_msgdef_ntof(m, Z_STRVAL_P(member), Z_STRLEN_P(member));

  if (!f) {
    php_error_docref(NULL, E_USER_ERROR,
                     "No such property %s.",
                     ZSTR_VAL(msg->desc->class_entry->name));
  }

  if (upb_fielddef_issubmsg(f)) {
    php_error_docref(NULL, E_USER_ERROR, "NYI");
    return NULL;
  }

  return f;
}

static zval *message_read_property(zval *obj, zval *member, int type,
                                   void **cache_slot, zval *rv) {
  Message* intern = (Message*)Z_OBJ_P(obj);
  const Descriptor *subdesc = NULL;
  const upb_fielddef *f = get_field(intern, member, &subdesc);
  upb_msgval msgval;

  if (!f) return NULL;

  msgval = upb_msg_get(intern->msg, f);
  pbphp_tozval(msgval, rv, upb_fielddef_type(f), NULL, &intern->arena);
  return rv;
}

static void message_write_property(zval *obj, zval *member, zval *val,
                                   void **cache_slot) {
  Message* intern = (Message*)Z_OBJ_P(obj);
  const Descriptor *subdesc = NULL;
  const upb_fielddef *f = get_field(intern, member, &subdesc);
  upb_arena *arena = arena_get(&intern->arena);
  upb_msgval msgval;

  if (!f ||
      !pbphp_tomsgval(val, &msgval, upb_fielddef_type(f), subdesc, arena)) {
    return;
  }

  upb_msg_set(intern->msg, f, msgval, arena);
}

upb_msg *pbphp_tomsg(zval *val, const Descriptor *desc, upb_arena *arena) {
  if (Z_TYPE_P(val) == IS_OBJECT &&
      instanceof_function(Z_OBJCE_P(val), desc->class_entry)) {
    Message *intern = (Message*)Z_OBJ_P(val);
    if (arena) {
      upb_arena_fuse(arena, arena_get(&intern->arena));
    }
    return intern->msg;
  } else {
    php_error_docref(NULL, E_USER_ERROR,
                     "Given value is not an instance of %s.",
                     ZSTR_VAL(desc->class_entry->name));
    return NULL;
  }
}

PHP_METHOD(Message, __construct) {
  Message* intern = (Message*)Z_OBJ_P(getThis());
  const Descriptor* desc = pupb_getdesc(Z_OBJCE_P(getThis()));
  const upb_msgdef *msgdef = desc->msgdef;
  upb_arena *arena = arena_get(&intern->arena);

  intern->desc = desc;
  intern->msg = upb_msg_new(msgdef, arena);
}

static  zend_function_entry message_methods[] = {
  //PHP_ME(Message, clear, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, discardUnknownFields, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, serializeToString, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, mergeFromString, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, serializeToJsonString, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, mergeFromJsonString, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, mergeFrom, NULL, ZEND_ACC_PUBLIC)
  //PHP_ME(Message, readWrapperValue, NULL, ZEND_ACC_PROTECTED)
  //PHP_ME(Message, writeWrapperValue, NULL, ZEND_ACC_PROTECTED)
  //PHP_ME(Message, readOneof, NULL, ZEND_ACC_PROTECTED)
  //PHP_ME(Message, writeOneof, NULL, ZEND_ACC_PROTECTED)
  //PHP_ME(Message, whichOneof, NULL, ZEND_ACC_PROTECTED)
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

PHP_METHOD(Message, clear) {
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
