#ifndef KAME_UTIL_FUNCTION_H
#define KAME_UTIL_FUNCTION_H

#include <cstddef>
#include <cstdlib>
#include <new>
#include <exception>
#include <type_traits>

namespace KameUtil {

struct Allocator {
  void* allocate(size_t n) 
  { 
    void *p = malloc(n); 
    if (!p) {
      throw std::bad_alloc();
    }
    return p;
  }

  void* allocate(size_t n, size_t align) 
  {
    return allocate(n);
  }

  void deallocate(void *p, size_t n) { free(p); }
};

template <class T, class Alloc = Allocator>
class Function;

template <class Alloc, class R, class ...Args>
class Function<R(Args...), Alloc> : Alloc {
public:
  Function() : base{nullptr} { }
  Function(std::nullptr_t) : base{nullptr} { }

  template <class Func>
  Function(Func f) 
  { 
    const bool needs_alloc = 
      sizeof(Child<Func>) > data_size;
    typedef typename std::conditional<needs_alloc, YesAlloc_t, 
      NoAlloc_t>::type MaybeAlloc_t;
    init(std::move(f), MaybeAlloc_t());
  }

  Function(const Function &other) 
  {
    if (other.base == (Base*)&other.data) {
      // Fits in data buffer, not allocated
      base = other.base->clone(data);
    } else if (other.base) {
      // Needs to be allocated
      StorageInfo si = other.base->storageInfo();
      base = (Base*) Alloc::allocate(si.size, si.align);
      other.base->clone(base);
    } else {
      // Other is null
      base = nullptr;
    }
  }

  Function(Function &&other) 
  {
    if (other.base == (Base*)&other.data) {
      // Fits in data buffer, not allocated
      base = other.base->move(data);
      other.reset();
    } else if (other.base) {
      // Needs to be allocated
      StorageInfo si = other.base->storageInfo();
      base = (Base*) Alloc::allocate(si.size, si.align);
      other.base->move(base);
      other.reset();
    } else {
      // Other is null
      base = nullptr;
    }
  }

  template <class Func>
  Function& operator=(Func f) 
  { 
    reset();
    const bool needs_alloc = 
      sizeof(Child<Func>) > data_size;
    typedef typename std::conditional<needs_alloc, YesAlloc_t, 
      NoAlloc_t>::type MaybeAlloc_t;
    init(std::move(f), MaybeAlloc_t());
    return *this;
  }

  Function& operator=(std::nullptr_t) { reset(); return *this; }

  Function& operator=(const Function &other) 
  {
    if (this != &other) {
      reset();
      if (other.base == (Base*)&other.data) {
        // Fits in data buffer, not allocated
        base = other.base->clone(data);
      } else if (other.base) {
        // Needs to be allocated
        StorageInfo si = other.base->storageInfo();
        base = (Base*) Alloc::allocate(si.size, si.align);
        other.base->clone(base);
      } else {
        // Other is null
        base = nullptr;
      }
    }
    return *this;
  }

  Function& operator=(Function &&other) 
  {
    if (this != &other) {
      reset();
      if (other.base == (Base*)&other.data) {
        // Fits in data buffer, not allocated
        base = other.base->move(data);
        other.reset();
      } else if (other.base) {
        // Needs to be allocated
        StorageInfo si = other.base->storageInfo();
        base = (Base*) Alloc::allocate(si.size, si.align);
        other.base->move(base);
        other.reset();
      } else {
        // Other is null
        base = nullptr;
      }
    }
    return *this;
  }

  ~Function() { reset(); }

  R operator()(Args ...args)
  {
    return base->call(std::forward<Args>(args)...);
  }

  explicit operator bool() const { return base != nullptr; }

  friend bool operator==(const Function &f, std::nullptr_t) 
  { 
    return !f; 
  }
  friend bool operator==(std::nullptr_t, const Function &f) 
  { 
    return !f; 
  }
  friend bool operator!=(const Function &f, std::nullptr_t) 
  { 
    return (bool)f; 
  }
  friend bool operator!=(std::nullptr_t, const Function &f) 
  { 
    return (bool)f; 
  }

  bool wasAllocated() const { return base && base != (Base*)&data; }

private:
  struct YesAlloc_t { };
  struct NoAlloc_t { };

  struct StorageInfo {
    size_t size;
    size_t align;
  };

  struct Base {
    virtual ~Base() { }
    virtual R call(Args ...args) = 0;
    virtual StorageInfo storageInfo() const = 0;
    virtual Base* clone(void *data) = 0;
    virtual Base* move(void *data) = 0;
  };

  template <class Func>
  struct Child : Base {
    Child(Func func) : func{std::move(func)} { }

    R call(Args ...args) override { return func(args...); }

    Base* clone(void *data) override
    {
      return new(data) Child(func);        
    }
 
    Base* move(void *data) override
    {
      return new(data) Child(std::move(func));        
    }
  
    StorageInfo storageInfo() const override
    {
      StorageInfo si = { sizeof(Child), alignof(Child) };
      return si;
    }

    Func func;
  };

  template <class Func>
  void init(Func f, NoAlloc_t)
  {
    base = new(&data) Child<Func>(std::move(f)); 
  }

  template <class Func>
  void init(Func f, YesAlloc_t)
  {
    base = (Base*)Alloc::allocate(sizeof(Child<Func>));
    new(base) Child<Func>(std::move(f)); 
  }

  void reset() 
  {
    if (base == (Base*)&data) {
      base->~Base();
    } else if (base) { 
      StorageInfo si = base->storageInfo();
      base->~Base();
      Alloc::deallocate(base, si.size);
    }
    base = nullptr;
  }

  // For sizeof vtable + func pointer + data pointer
  struct DummyT {
    virtual ~DummyT() { }
    void (*f)();
    void *d;
  };

  static const int data_size = sizeof(DummyT);
  alignas(std::max_align_t) char data[data_size];
  Base *base;
};

}

#endif
