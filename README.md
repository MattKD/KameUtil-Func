# cpp_function
Polymorphic function in C++11

For fun I wrote an almost clone of std::function. It has the same interface as std::function except in a few cases:

1) It has an allocator as part of type instead of passed to constructor,
```
KameUtil::Function<void(int), SomeAllocator> f;
std::function<void(int)> f(std::allocator_arg_t, SomeAllocator());
```

2) The Allocator type doesn't use the same interface as the STL Allocator concept.
```
struct Allocator {
  void* allocate(size_t n);
  void* allocate(size_t n, size_t align);
  void deallocate(void *p, size_t n);
};
```

3) target and target_type methods aren't implemented.

4) KameUtil::Function guarantees size for a function pointer + data pointer without allocating; std::function only for a function pointer.
