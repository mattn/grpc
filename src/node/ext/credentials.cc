/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <node.h>

#include "grpc/grpc.h"
#include "grpc/grpc_security.h"
#include "grpc/support/log.h"
#include "credentials.h"

namespace grpc {
namespace node {

using ::node::Buffer;
using v8::Arguments;
using v8::Exception;
using v8::External;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::Value;

Persistent<Function> Credentials::constructor;
Persistent<FunctionTemplate> Credentials::fun_tpl;

Credentials::Credentials(grpc_credentials *credentials)
    : wrapped_credentials(credentials) {}

Credentials::~Credentials() {
  grpc_credentials_release(wrapped_credentials);
}

void Credentials::Init(Handle<Object> exports) {
  NanScope();
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(NanNew("Credentials"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  NanAssignPersistent(fun_tpl, tpl);
  NanAssignPersistent(constructor, tpl->GetFunction());
  constructor->Set(NanNew("createDefault"),
                   FunctionTemplate::New(CreateDefault)->GetFunction());
  constructor->Set(NanNew("createSsl"),
                   FunctionTemplate::New(CreateSsl)->GetFunction());
  constructor->Set(NanNew("createComposite"),
                   FunctionTemplate::New(CreateComposite)->GetFunction());
  constructor->Set(NanNew("createGce"),
                   FunctionTemplate::New(CreateGce)->GetFunction());
  constructor->Set(NanNew("createFake"),
                   FunctionTemplate::New(CreateFake)->GetFunction());
  constructor->Set(NanNew("createIam"),
                   FunctionTemplate::New(CreateIam)->GetFunction());
  exports->Set(NanNew("Credentials"), constructor);
}

bool Credentials::HasInstance(Handle<Value> val) {
  NanScope();
  return NanHasInstance(fun_tpl, val);
}

Handle<Value> Credentials::WrapStruct(grpc_credentials *credentials) {
  NanEscapableScope();
  if (credentials == NULL) {
    return NanEscapeScope(NanNull());
  }
  const int argc = 1;
  Handle<Value> argv[argc] = {
      External::New(reinterpret_cast<void *>(credentials))};
  return NanEscapeScope(constructor->NewInstance(argc, argv));
}

grpc_credentials *Credentials::GetWrappedCredentials() {
  return wrapped_credentials;
}

NAN_METHOD(Credentials::New) {
  NanScope();

  if (args.IsConstructCall()) {
    if (!args[0]->IsExternal()) {
      return NanThrowTypeError(
          "Credentials can only be created with the provided functions");
    }
    grpc_credentials *creds_value =
        reinterpret_cast<grpc_credentials *>(External::Unwrap(args[0]));
    Credentials *credentials = new Credentials(creds_value);
    credentials->Wrap(args.This());
    NanReturnValue(args.This());
  } else {
    const int argc = 1;
    Local<Value> argv[argc] = {args[0]};
    NanReturnValue(constructor->NewInstance(argc, argv));
  }
}

NAN_METHOD(Credentials::CreateDefault) {
  NanScope();
  NanReturnValue(WrapStruct(grpc_google_default_credentials_create()));
}

NAN_METHOD(Credentials::CreateSsl) {
  NanScope();
  char *root_certs = NULL;
  grpc_ssl_pem_key_cert_pair key_cert_pair = {NULL, NULL};
  if (Buffer::HasInstance(args[0])) {
    root_certs = Buffer::Data(args[0]);
  } else if (!(args[0]->IsNull() || args[0]->IsUndefined())) {
    return NanThrowTypeError("createSsl's first argument must be a Buffer");
  }
  if (Buffer::HasInstance(args[1])) {
    key_cert_pair.private_key = Buffer::Data(args[1]);
  } else if (!(args[1]->IsNull() || args[1]->IsUndefined())) {
    return NanThrowTypeError(
        "createSSl's second argument must be a Buffer if provided");
  }
  if (Buffer::HasInstance(args[2])) {
    key_cert_pair.cert_chain = Buffer::Data(args[2]);
  } else if (!(args[2]->IsNull() || args[2]->IsUndefined())) {
    return NanThrowTypeError(
        "createSSl's third argument must be a Buffer if provided");
  }

  NanReturnValue(WrapStruct(grpc_ssl_credentials_create(
      root_certs, key_cert_pair.private_key == NULL ? NULL : &key_cert_pair)));
}

NAN_METHOD(Credentials::CreateComposite) {
  NanScope();
  if (!HasInstance(args[0])) {
    return NanThrowTypeError(
        "createComposite's first argument must be a Credentials object");
  }
  if (!HasInstance(args[1])) {
    return NanThrowTypeError(
        "createComposite's second argument must be a Credentials object");
  }
  Credentials *creds1 = ObjectWrap::Unwrap<Credentials>(args[0]->ToObject());
  Credentials *creds2 = ObjectWrap::Unwrap<Credentials>(args[1]->ToObject());
  NanReturnValue(WrapStruct(grpc_composite_credentials_create(
      creds1->wrapped_credentials, creds2->wrapped_credentials)));
}

NAN_METHOD(Credentials::CreateGce) {
  NanScope();
  NanReturnValue(WrapStruct(grpc_compute_engine_credentials_create()));
}

NAN_METHOD(Credentials::CreateFake) {
  NanScope();
  NanReturnValue(WrapStruct(grpc_fake_transport_security_credentials_create()));
}

NAN_METHOD(Credentials::CreateIam) {
  NanScope();
  if (!args[0]->IsString()) {
    return NanThrowTypeError("createIam's first argument must be a string");
  }
  if (!args[1]->IsString()) {
    return NanThrowTypeError("createIam's second argument must be a string");
  }
  NanUtf8String auth_token(args[0]);
  NanUtf8String auth_selector(args[1]);
  NanReturnValue(
      WrapStruct(grpc_iam_credentials_create(*auth_token, *auth_selector)));
}

}  // namespace node
}  // namespace grpc
