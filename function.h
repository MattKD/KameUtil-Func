#ifndef KAME_UTIL_FUNCTION_H
#define KAME_UTIL_FUNCTION_H

#include <cstddef>
#include <cstdlib>
#include <new>
#include <exception>
#include <type_traits>

namespace KameUtil {

struct DefaultFuncAlloc {
  void* allocate(size_t n) 
  { 
    void *p = malloc(n); 
    if (!p) {
      throw std::bad_alloc();
    }
    return p;
  }

  void deallocate(void *p, size_t n) noexcept
  { 
    free(p); 
  }
};

template <class T>
class Function;

template <class R, class ...Args>
class Function<R(Args...)> {
public:
  Function() noexcept : base{nullptr} { }
  Function(std::nullptr_t) noexcept : base{nullptr} { }

  template <class Func, class Alloc = DefaultFuncAlloc>
  Function(Func f, Alloc alloc = Alloc())
  { 
    const bool needs_alloc = 
      sizeof(Child<Func, Alloc>) > data_size;
    typedef typename std::conditional<needs_alloc, YesAlloc_t, 
      NoAlloc_t>::type MaybeAlloc_t;

    init(std::move(f), MaybeAlloc_t(), std::move(alloc));
  }

  Function(const Function &other) 
  {
    if (other.base == (Base*)&other.data) {
      // Fits in data buffer, not allocated
      base = other.base->clone(data);
    } else if (other.base) {
      // Needs to be allocated
      std::size_t size = other.base->size();
      base = (Base*) other.base->alloc(size);
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
      base = other.base;
      other.base = nullptr;
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
      sizeof(Child<Func, DefaultFuncAlloc>) > data_size;
    typedef typename std::conditional<needs_alloc, YesAlloc_t, 
      NoAlloc_t>::type MaybeAlloc_t;

    init(std::move(f), MaybeAlloc_t(), DefaultFuncAlloc());
    return *this;
  }

  Function& operator=(std::nullptr_t) noexcept  
  { 
    reset(); 
    return *this; 
  }

  Function& operator=(const Function &other) 
  {
    if (this != &other) {
      reset();
      if (other.base == (Base*)&other.data) {
        // Fits in data buffer, not allocated
        base = other.base->clone(data);
      } else if (other.base) {
        // Needs to be allocated
        std::size_t size = other.base->size();
        base = (Base*) other.base->alloc(size);
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
        base = other.base;
        other.base = nullptr;
      } else {
        // Other is null
        base = nullptr;
      }
    }
    return *this;
  }

  ~Function() noexcept { reset(); }

  R operator()(Args&& ...args)
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

  struct Base {
    virtual ~Base() { }
    virtual R call(Args&& ...args) = 0;
    virtual std::size_t size() const = 0;
    virtual Base* clone(void *data) = 0;
    virtual Base* move(void *data) = 0;
    virtual void* alloc(size_t size) = 0;
    virtual void deallocSelf() = 0;
  };

  template <class Func, class Alloc>
  struct Child : Base, private Alloc {
    Child(const Func &func, const Alloc &alloc) 
      : func{func}, Alloc(alloc) { }

    Child(Func &&func, Alloc &&alloc) 
      : func{std::move(func)}, Alloc(std::move(alloc)) { }

    R call(Args&& ...args) override 
    { 
      return func(std::forward<Args>(args)...); 
    }

    Base* clone(void *data) override
    {
      Alloc &alloc = static_cast<Alloc&>(*this);
      return new(data) Child(func, alloc);        
    }
 
    Base* move(void *data) override
    {
      Alloc &alloc = static_cast<Alloc&>(*this);
      return new(data) Child(std::move(func), std::move(alloc));        
    }
  
    std::size_t size() const override
    {
      return sizeof(Child);
    }

    void* alloc(size_t size) override 
    {
      return Alloc::allocate(size);
    }

    void deallocSelf() override 
    {
      // Alloc stored in Child, so copy first
      Alloc alloc = static_cast<Alloc&>(*this);
      this->~Child<Func, Alloc>(); // destroy self
      alloc.deallocate(this, sizeof(Child<Func, Alloc>));
    }

    Func func;
  };

  template <class Func, class Alloc>
  void init(Func &&f, NoAlloc_t, Alloc &&alloc)
  {
    base = new(&data) Child<Func, Alloc>(std::forward<Func>(f), 
                                         std::forward<Alloc>(alloc)); 
  }

  template <class Func, class Alloc>
  void init(Func &&f, YesAlloc_t, Alloc &&alloc)
  {
    base = (Base*) alloc.allocate(sizeof(Child<Func, Alloc>));
    new(base) Child<Func, Alloc>(std::forward<Func>(f), 
                                 std::forward<Alloc>(alloc)); 
  }

  void reset() 
  {
    if (base == (Base*)&data) {
      base->~Base();
    } else if (base) { 
      base->deallocSelf(); // calls dtor and deallocates
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
