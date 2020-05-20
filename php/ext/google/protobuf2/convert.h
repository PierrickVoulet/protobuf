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

#ifndef PHP_PROTOBUF_CONVERT_H_
#define PHP_PROTOBUF_CONVERT_H_

#include <php.h>

#include "php-upb.h"
#include "def.h"

upb_fieldtype_t pbphp_dtype_to_type(upb_descriptortype_t type);
bool pbphp_toi64(zval *php_val, int64_t *i64);
bool pbphp_tomsgval(zval *php_val, upb_msgval *upb_val, upb_fieldtype_t type,
                    const Descriptor *desc, upb_arena *arena);
void pbphp_tozval(upb_msgval upb_val, zval *php_val, upb_fieldtype_t type,
                  const Descriptor *desc, zval *arena);

bool pbphp_initarray(upb_array *arr, const upb_fielddef *f, zval *val,
                     upb_arena *arena);
bool pbphp_initmap(upb_map *map, const upb_fielddef *f, zval *val,
                   upb_arena *arena);
bool pbphp_initmsg(upb_msg *msg, const upb_msgdef *m, zval *init,
                   upb_arena *arena);

void convert_module_init(void);

#endif  // PHP_PROTOBUF_CONVERT_H_
