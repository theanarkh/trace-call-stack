#include <node.h>
#include <uv.h>

namespace test {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Context;

static uv_sem_t sem;

void Wait(const FunctionCallbackInfo<Value>& args) {
  uv_sem_init(&sem, 0);
  uv_sem_wait(&sem);
}

void Initialize(
  Local<Object> exports,
  Local<Value> module,
  Local<Context> context
) {
  NODE_SET_METHOD(exports, "wait", Wait);
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)

}  // namespace test
