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

using Matrix = std::vector<std::vector<double>>;

Matrix multiply(const Matrix& a, const Matrix& b) {
    int n = a.size();
    int m = a[0].size();
    int k = b[0].size();
    Matrix c(n, std::vector<double>(k, 0.0));

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < k; ++j) {
            for (int p = 0; p < m; ++p) {
                c[i][j] += a[i][p] * b[p][j]; // 三重循环，计算量极大
            }
        }
    }
    return c;
}

void Compute(const FunctionCallbackInfo<Value>& args) {
  Matrix a(1000, std::vector<double>(1000, 5.0));
  Matrix b(1000, std::vector<double>(1000, 10.0));
  auto c = multiply(a, b);
}

void Initialize(
  Local<Object> exports,
  Local<Value> module,
  Local<Context> context
) {
  NODE_SET_METHOD(exports, "compute", Compute);
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)

}  // namespace test
