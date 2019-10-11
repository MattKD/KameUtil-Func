# cpp-func
Polymorphic function in C++11

For fun I wrote an almost clone of std::function. It has the same interface as std::function except in a few cases:

1) std::functin allocator didn't have specified behavior on how they were stored/copied, and have been removed in C++17. KameUtil::Function allocator is stored with function object when constructor or assign operator are called. If a Function f is assigned to nullptr it doesn't have an allocator; if assigned to Function f2 then f will have same allocator as f2; if assigned to a function object it will use the KameUtil::DefaultFuncAlloc which calls malloc/free.
```
Function<void()> f(someFunc, MyAlloc()); // uses MyAlloc::allocate if someFunc is bigger than 2 pointers 
auto f2 = f; // f2 get copy of someFunc and MyAlloc
auto voidFunc = []() { }; 
f = voidFunc; // now has voidFunc and KameUtil::DefaultFuncAlloc
f2 = nullptr; // no allocator stored
f2 = std::move(f); // f2 has voidFunc and DefaultFuncAlloc, f has no Allocator or stored function
```

2) The Allocator type doesn't use the same interface as the STL Allocator concept.
```
struct Allocator {
  void* allocate(size_t n);
  void deallocate(void *p, size_t n);
};
```

3) target and target_type methods aren't implemented.

4) KameUtil::Function guarantees size for a function pointer + data pointer without allocating; std::function only for a function pointer.
