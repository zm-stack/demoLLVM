dynamic_cast-like operator
---

* isa<>: returns true or false depending on whether a reference or pointer points to an instance of the specified class.
* cast<>: converts a pointer or reference from a base class to a derived class.  
Do not use an isa<> test followed by a cast<>, for that use the dyn_cast<> operator.
```C++
static bool isLoopInvariant(const Value *V, const Loop *L) {
if (isa<Constant>(V) || isa<Argument>(V) || isa<GlobalValue>(V))
    return true;

// Otherwise, it must be an instruction...
return !L->contains(cast<Instruction>(V)->getParent());
}
```
* dyn_cast<>: if the operand is of the specified type, returns a pointer to it.Otherwise, a null pointer is returned. (Do not use big chained if/then/else blocks to check for lots of different variants of classes. Use the InstVisitor class to dispatch over the instruction type directly.)
```C++
if (auto *AI = dyn_cast<AllocationInst>(Val)) {
  // ...
}
```
* isa_and_nonnull<>: works just like the isa<> operator, except that it allows for a null pointer as an argument (which it then returns false), allowing you to combine several null checks into one.
* cast_or_null<>: works just like the cast<> operator, except that it allows for a null pointer as an argument (which it then propagates).
* dyn_cast_or_null<>: works just like the dyn_cast<> operator, except that it allows for a null pointer as an argument (which it then propagates).

***

Passing strings (StringRef and Twine)
---

### The StringRef class
Represents a reference to a constant string (a character array and a length) and supports the common operations available on std::string, but does not require heap allocation.

    iterator find(StringRef Key);
    Map.find("foo");                 // Lookup "foo"
    Map.find(std::string("bar"));    // Lookup "bar"
    Map.find(StringRef("\0baz", 4)); // Lookup "\0baz"
APIs which need to return a string may return a StringRef instance, which can be used directly or converted to an std::string using the str member function. You should rarely use the StringRef class directly, because it contains pointers to external memory. It should always be passed by value.

### The Twine class
An efficient way for APIs to accept concatenated strings:  
`New = CmpInst::Create(..., SO->getName() + ".cmp");`  
Twines can be implicitly constructed as the result of the plus operator applied to strings (i.e., a C strings, an std::string, or a StringRef). Twine objects point to external memory and should almost never be stored or mentioned directly. 

### Formatting strings (formatv)
A call to formatv involves a single format string consisting of 0 or more replacement sequences(a string of the form {N[[,align]:style]}), followed by a variable length list of replacement values.  
There are two ways to customize the formatting behavior for a type.
```C++
namespace llvm {
  template<>
  struct format_provider<MyFooBar> {
    static void format(const MyFooBar &V, raw_ostream &Stream, StringRef Style) {
      // Do whatever is necessary to format `V` into `Stream`
    }
  };
  void foo() {
    MyFooBar X;
    std::string S = formatv("{0}", X);
  }
}
```
```C++
namespace anything {
  struct format_int_custom : public llvm::FormatAdapter<int> {
    explicit format_int_custom(int N) : llvm::FormatAdapter<int>(N) {}
    void format(llvm::raw_ostream &Stream, StringRef Style) override {
      // Do whatever is necessary to format ``this->Item`` into ``Stream``
    }
  };
}
namespace llvm {
  void foo() {
    std::string S = formatv("{0}", anything::format_int_custom(42));
  }
}
```
If the type is detected to be derived from FormatAdapter<T>, formatv will call the format method on the argument passing in the specified style. This allows one to provide custom formatting of any type, including one which already has a builtin format provider.  
*More formatv Examples:*  
```c++
std::string S;
// Simple formatting of basic types and implicit string conversion.
S = formatv("{0} ({1:P})", 7, 0.35);  // S == "7 (35.00%)"
// Out-of-order referencing and multi-referencing
outs() << formatv("{0} {2} {1} {0}", 1, "test", 3); // prints "1 3 test 1"
// Left, right, and center alignment
S = formatv("{0,7}",  'a');  // S == "      a";
S = formatv("{0,-7}", 'a');  // S == "a      ";
S = formatv("{0,=7}", 'a');  // S == "   a   ";
S = formatv("{0,+7}", 'a');  // S == "      a";
// Custom styles
S = formatv("{0:N} - {0:x} - {1:E}", 12345, 123908342); // S == "12,345 - 0x3039 - 1.24E8"
// Adapters
S = formatv("{0}", fmt_align(42, AlignStyle::Center, 7));  // S == "  42   "
S = formatv("{0}", fmt_repeat("hi", 3)); // S == "hihihi"
S = formatv("{0}", fmt_pad("hi", 2, 6)); // S == "  hi      "
// Ranges
std::vector<int> V = {8, 9, 10};
S = formatv("{0}", make_range(V.begin(), V.end())); // S == "8, 9, 10"
S = formatv("{0:$[+]}", make_range(V.begin(), V.end())); // S == "8+9+10"
S = formatv("{0:$[ + ]@[x]}", make_range(V.begin(), V.end())); // S == "0x8 + 0x9 + 0xA"
```

***

llvm/ADT/ directory
---
* map-like container: for look-up of a value based on another value, or queries for containment.
* set-like container: automatically eliminates duplicates.
* sequential container: efficient way to add elements and keeps track of the order they are added to the collection.
* string container: for character or byte arrays.
* bit container: store and perform set operations on sets of numeric id’s, while automatically eliminating duplicates.
***
https://llvm.org/docs/ProgrammersManual.html#picking-the-right-data-structure-for-a-task