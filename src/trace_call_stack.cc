#include <node.h>
#include <uv.h>
#include <unistd.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <sstream>

// Code from Node.js
namespace trace_call_stack {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Context;
using v8::StackFrame;
using v8::StackTrace;
using v8::Message;

uv_sem_t sem;

class SymbolInfo {
  public:
   std::string name;
   std::string filename;
   size_t line = 0;
   size_t dis = 0;
   std::string Display() const  {
    std::ostringstream oss;
    oss << name;
    if (dis != 0) {
      oss << "+" << dis;
    }
    if (!filename.empty()) {
      oss << " [" << filename << ']';
    }
    if (line != 0) {
      oss << ":L" << line;
    }
    return oss.str();
  }
 };

SymbolInfo LookupSymbol(void* address)  {
  Dl_info info;
  const bool have_info = dladdr(address, &info);
  SymbolInfo ret;
  if (!have_info)
    return ret;

  if (info.dli_sname != nullptr) {
    if (char* demangled =
            abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, nullptr)) {
      ret.name = demangled;
      free(demangled);
    } else {
      ret.name = info.dli_sname;
    }
  }

  if (info.dli_fname != nullptr) {
    ret.filename = info.dli_fname;
  }

  return ret;
}

void handler(int signum,
  siginfo_t* info,
  void* ucontext) {
  int fd = open("./trace_cpp_call_stack.log", O_RDWR | O_TRUNC | O_CREAT, 0777);

  void* buffer[256];
  const int size = backtrace(buffer, 256);

  if(size <= 0)
  {
    const char * msg = "failed to get call statck";
    write(fd, msg, strlen(msg));
    return;
  }

  for (int i = 1; i < size; i++) {
    void* frame = buffer[i];
    std::string data = LookupSymbol(frame).Display() + "\n";
    ssize_t written = write(fd,data.c_str(),data.size());
    if (written != ssize_t(data.size())) {
      const char * msg = "failed to write call stack";
      write(fd, msg, strlen(msg));
    }
  }
  close(fd);
  uv_sem_post(&sem);
}

enum class StackTracePrefix {
  kAt,  // "    at "
  kNumber
};

static void PrintToStderrAndFlush(const std::string& str) {
  int fd = open("./trace_js_call_stack.log", O_RDWR | O_TRUNC | O_CREAT, 0777);
  ssize_t written = write(fd,str.c_str(),str.size());
  if (written != ssize_t(str.size())) {
    const char * msg = "failed to write call stack";
    write(fd, msg, strlen(msg));
  }
  close(fd);
}

static std::string FormatStackTrace(
  Isolate* isolate,
  Local<StackTrace> stack,
  StackTracePrefix prefix = StackTracePrefix::kAt) {
  std::string result;
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(isolate, i);
    v8::String::Utf8Value fn_name_s(isolate, stack_frame->GetFunctionName());
    v8::String::Utf8Value script_name(isolate, stack_frame->GetScriptName());
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();
    std::string prefix_str = prefix == StackTracePrefix::kAt
                                ? "    at "
                                : std::to_string(i + 1) + ": ";
    if (stack_frame->IsEval()) {
      std::vector<char> buf(script_name.length() + 64);
      snprintf(buf.data(),
              buf.size(),
              "%s[eval] (%s:%i:%i)\n",
              prefix_str.c_str(),
              *script_name,
              line_number,
              column);
      result += std::string(buf.data());
      break;
    }

    if (fn_name_s.length() == 0) {
      std::vector<char> buf(script_name.length() + 64);
      snprintf(buf.data(),
              buf.size(),
              "%s%s:%i:%i\n",
              prefix_str.c_str(),
              *script_name,
              line_number,
              column);
      result += std::string(buf.data());
    } else {
      std::vector<char> buf(fn_name_s.length() + script_name.length() + 64);
      snprintf(buf.data(),
              buf.size(),
              "%s%s (%s:%i:%i)\n",
              prefix_str.c_str(),
              *fn_name_s,
              *script_name,
              line_number,
              column);
      result += std::string(buf.data());
    }
  }
  return result;
}

void PrintStackTrace(Isolate* isolate,
  Local<StackTrace> stack,
  StackTracePrefix prefix = StackTracePrefix::kAt) {
  PrintToStderrAndFlush(FormatStackTrace(isolate, stack, prefix));
}

void* Worker(void* arg) {
  Isolate* isolate = static_cast<Isolate*>(arg);
  do {
    uv_sem_wait(&sem);
    isolate->RequestInterrupt(
      [](v8::Isolate* isolate, void* data) {
        PrintStackTrace(isolate,
                    v8::StackTrace::CurrentStackTrace(
                        isolate, 10, v8::StackTrace::kDetailed));
      },
      isolate);
  } while (1);

  return nullptr;
}

void Install(const FunctionCallbackInfo<Value>& args) {
  uv_sem_init(&sem, 0);
  Isolate* isolate = args.GetIsolate();
  sigset_t sigmask;
  sigfillset(&sigmask);
  sigset_t oldmask;
  pthread_sigmask(SIG_SETMASK, &sigmask, &oldmask);
  pthread_t thread;
  int ret = pthread_create(&thread, nullptr, Worker, isolate);
  pthread_sigmask(SIG_SETMASK, &oldmask, nullptr);
  if (ret != 0) {
    printf("failed to create thread");
    return;
  }
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = handler;
  sa.sa_flags = 0;
  sigfillset(&sa.sa_mask);
  sigaction(SIGINT, &sa, nullptr);
}

void Initialize( Local<Object> exports, Local<Value> module, Local<Context> context) {
  NODE_SET_METHOD(exports, "install", Install);
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)

}  // namespace trace_call_stack
