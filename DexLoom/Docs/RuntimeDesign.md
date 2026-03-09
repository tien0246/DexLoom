# DexLoom Runtime Design

## Object Model

### DxObject
Every heap-allocated entity. Contains:
- `class_ref`: pointer to DxClass metadata
- `fields[]`: array of DxValue slots (sized per class field count)
- `ref_count`: reference count for memory management
- `flags`: GC/mark bits, lock state placeholder

### DxClass
Loaded from DEX class_def. Contains:
- `descriptor`: e.g. "Lcom/example/MainActivity;"
- `super_class`: pointer to parent DxClass
- `interfaces[]`: (v1: ignored)
- `static_fields[]`: DxValue array
- `instance_field_count`: determines DxObject.fields[] size
- `field_descriptors[]`: name + type for each field
- `direct_methods[]`: constructors, private, static methods
- `virtual_methods[]`: overridable methods (vtable order)
- `vtable[]`: flattened virtual method table (inherited + own)
- `class_status`: NOT_LOADED, LOADING, LOADED, INITIALIZED, ERROR
- `static_initializer`: method ref for <clinit> if present

### DxMethod
- `class_ref`: owning class
- `name`: method name string
- `proto`: return type + parameter types
- `access_flags`: public/private/static/etc.
- `code`: pointer to DxCodeItem (bytecode + metadata)
- `native_impl`: function pointer for bridge methods (framework stubs)
- `is_native`: true for framework methods implemented in C

### DxValue (tagged union)
```c
typedef struct {
    enum { DX_VAL_INT, DX_VAL_LONG, DX_VAL_FLOAT, DX_VAL_DOUBLE, DX_VAL_OBJ, DX_VAL_NULL } tag;
    union {
        int32_t i;
        int64_t l;
        float f;
        double d;
        DxObject* obj;
    };
} DxValue;
```

## Memory Management

iOS constraints rule out:
- Custom mmap with PROT_EXEC (JIT)
- Large contiguous heaps (must work within app memory limits)

Strategy: **Reference counting + arena allocator**
- Objects allocated from a pool/arena
- Reference counted (no cycles expected in v1 demo apps)
- Framework objects (Views, etc.) prevent cycles by design
- If cycles become a problem in v2: add mark-sweep on top

## Interpreter Design

Register-based, matching DEX's register model:
- Each method frame has `registers[N]` where N = code_item.registers_size
- Parameters occupy the last M registers (M = param count + 1 for `this`)
- Local variables occupy registers 0..N-M-1

Dispatch: switch-based (computed goto is a v2 optimization)

```
while (pc < code_end) {
    uint16_t inst = code[pc];
    uint8_t opcode = inst & 0xFF;
    switch (opcode) {
        case OP_CONST_4: ...
        case OP_INVOKE_VIRTUAL: ...
        ...
    }
}
```

## Method Resolution

1. For invoke-virtual: look up method in receiver's vtable by vtable index
2. For invoke-direct: look up in class's direct_methods (constructors, private)
3. For invoke-static: look up in class's direct_methods (static)
4. For invoke-super: look up in super_class vtable
5. For invoke-interface: linear scan of class's interfaces (v1 simplification)

## Class Loading

Lazy loading triggered by:
- new-instance opcode
- invoke-static (triggers <clinit>)
- First field access

Loading steps:
1. Find class_def in DEX by descriptor
2. Parse field and method lists
3. Load super class recursively
4. Build vtable (copy super vtable, append/override own virtuals)
5. Set status to LOADED
6. Run <clinit> if present -> status INITIALIZED

## String Handling

DEX strings are MUTF-8. Runtime converts to:
- C strings (UTF-8) for internal use
- Wrapped in DxObject with class "Ljava/lang/String;" for guest code
- String pool with deduplication for DEX string table entries

## Exception Model (v1 simplified)

- try/catch ranges stored per code item
- On exception: scan catch handlers for matching type
- If found: set PC to handler, put exception in designated register
- If not found: unwind frame, repeat in caller
- Uncaught: abort with error log

## Framework Method Bridge

Android framework methods (e.g., Activity.setContentView) are implemented as
native C functions. When the interpreter encounters an invoke on a bridge method:

1. Check method.is_native
2. Call method.native_impl(vm, frame, args)
3. Native impl performs the action (e.g., parse layout, build UI tree)
4. Return value placed in frame via move-result

This is how the interpreter talks to the "Android framework" without
actually having Android - each framework method is a C function that
simulates the expected behavior.
