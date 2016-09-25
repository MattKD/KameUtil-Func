#include "function.h"
#include <cstdint>
#include <iostream>
#include <functional>

static bool error_found = false;
void logError_(bool expr, const char *msg, const char *file, int line)
{
  if (!expr) {
    std::cerr << "Error: " << msg << ", in file " << file 
      << ", line " << line << "\n";
    error_found = true;
  }
}

#define logError(expr) logError_(expr, #expr, __FILE__, __LINE__)

int plus1(int n)
{
  return n+1;
}

struct PlusX {
  PlusX(int a) : x{a} { ctor_count += 1; }
  PlusX(const PlusX &other) : x{other.x} { ctor_count += 1; }
  PlusX(PlusX &&other) : x{other.x} { ctor_count += 1; }
  ~PlusX() { ctor_count -= 1; }
  int operator()(int n) { return n + x; }

  int x;
  //static int n;
  static int ctor_count;
};
int PlusX::ctor_count = 0;

struct LargeFunc {
  LargeFunc(int64_t a, int64_t b, int64_t c, int64_t d) 
    : a{a}, b{b}, c{c}, d{d} { ctor_count += 1; }
  LargeFunc(const LargeFunc &other) 
    : a{other.a}, b{other.b}, c{other.c}, d{other.d} { ctor_count += 1; }
  LargeFunc(LargeFunc &&other)
    : a{other.a}, b{other.b}, c{other.c}, d{other.d} { ctor_count += 1; }
  ~LargeFunc() { ctor_count -= 1; }
  int64_t operator()() { return a + b + c + d; }

  int64_t a, b, c, d;
  static int ctor_count;
};
int LargeFunc::ctor_count = 0;

struct FuncBuf {
  void (*func)();
  void *data;
};
const int FuncBufSize = sizeof(FuncBuf);

int main()
{
  using KameUtil::Function;
  using std::function;
  using std::move;
  using std::cout;

  // check function pointer
  {
    Function<int(int)> f(plus1);
    logError(f(1) == 2);

    if (sizeof(&plus1) <= FuncBufSize) {
      logError(!f.wasAllocated());
    } else {
      logError(false && "Expected to fit within Function");
      logError(f.wasAllocated());
    }
  }

  // check void return
  {
    int n = 1;
    auto lf = [&n](int x) { n = x; };
    Function<void(int)> f(lf);
    f(2);
    logError(n == 2);

    if (sizeof(lf) <= FuncBufSize) {
      logError(!f.wasAllocated());
    } else {
      logError(false && "Expected to fit within Function");
      logError(f.wasAllocated());
    }
  }

  // check function pointer with reference
  {
    int n = 9;
    int (*plus1_ptr)(int n) = plus1;
    auto lf = [plus1_ptr, n]() { return plus1_ptr(n); };
    Function<int()> f(lf);
    logError(f() == 10);

    if (sizeof(lf) <= FuncBufSize) {
      logError(!f.wasAllocated());
    } else {
      logError(false && "Expected to fit within Function");
      logError(f.wasAllocated());
    }
  }

  // check Function object, and number of ctors calls == dtor calls
  {
    logError(PlusX::ctor_count == 0);
    auto f = Function<int(int)>(PlusX(5));
    logError(f(3) == 8);
    logError(PlusX::ctor_count == 1);

    if (sizeof(PlusX) <= FuncBufSize) {
      logError(!f.wasAllocated());
    } else {
      logError(false && "Expected to fit within Function");
      logError(f.wasAllocated());
    }
  }
  logError(PlusX::ctor_count == 0);

  // check that large function is allocated, and 
  // number of ctors calls == dtor calls
  {
    logError(LargeFunc::ctor_count == 0);
    Function<int64_t()> f(LargeFunc(1,2,3,4));
    logError(f() == 10);
    logError(LargeFunc::ctor_count == 1);

    if (sizeof(LargeFunc) <= FuncBufSize) {
      logError(false && "Expected to be allocated within Function");
      logError(!f.wasAllocated());
    } else {
      logError(f.wasAllocated());
    }
  }
  logError(LargeFunc::ctor_count == 0);

  // check copy/move ctors and assign operators
  {
    Function<int(int)> f(plus1);
    logError(f != nullptr);
    logError(nullptr != f);

    auto f2 = f; // copy ctor
    logError(f2(1) == 2);
    f2 = f; // assign
    logError(f != nullptr);
    logError(f2(1) == 2);
    auto f3 = move(f); // move ctor, f is null
    logError(f3(1) == 2);
    logError(f == nullptr);
    f = move(f2); // move assign
    logError(f(1) == 2);
    logError(f2 == nullptr);
    
    Function<int(int)> f4 = nullptr; // null ctor
    logError(f4 == nullptr);
    logError(nullptr == f4);

    f4 = plus1; // assign new func
    logError((bool)f4);
    logError(f4(1) == 2);

    f4 = nullptr; // assign null
    logError(!f4);

    Function<void(), KameUtil::Allocator> f5; // set an allocator
    logError(f5 == nullptr);
  }

  typedef Function<void()> FuncT;
  typedef std::function<void()> StdFuncT;
  cout << "sizeof(Function): " << sizeof(FuncT) << "\n";
  cout << "sizeof(std::function): " << sizeof(StdFuncT) << "\n";
  cout << "alignof(Function): " << alignof(FuncT) << "\n";
  cout << "alignof(std::function): " << alignof(StdFuncT) << "\n";
  return 0;
}