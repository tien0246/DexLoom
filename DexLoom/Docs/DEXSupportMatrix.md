# DEX Support Matrix

## Opcode Coverage: All 256 Dalvik Opcodes

DexLoom implements **production-grade coverage of all 256 Dalvik opcodes** as defined in the
Android DEX format specification. This includes every instruction format, all arithmetic/logic
operations, array operations, comparisons, conversions, wide (64-bit) operations, switches,
fill-array-data (including 8-byte elements), monitor-enter/exit, throw, and all /range variants.

### Edge Cases Handled
- INT_MIN / -1 overflow in div-int and rem-int
- Null operands in instance-of and check-cast
- Fill-array-data with 8-byte wide elements
- Packed-switch and sparse-switch with full jump table parsing
- Finally block execution during exception unwinding

## Instruction Formats Supported

All DEX instruction formats are supported:

| Format | Description | Example Opcodes |
|--------|-------------|-----------------|
| 10x | op | nop, return-void |
| 10t | op +AA | goto |
| 11n | op vA, #+B | const/4 |
| 11x | op vAA | move-result, return, throw, monitor-* |
| 12x | op vA, vB | move, int-to-*, neg-*, not-* |
| 20t | op +AAAA | goto/16 |
| 21s | op vAA, #+BBBB | const/16, const-wide/16 |
| 21h | op vAA, #+BBBB0000 | const/high16, const-wide/high16 |
| 21t | op vAA, +BBBB | if-*z |
| 21c | op vAA, type/string@BBBB | const-string, new-instance, check-cast, etc. |
| 22x | op vAA, vBBBB | move/from16 |
| 22b | op vAA, vBB, #+CC | add-int/lit8, etc. |
| 22s | op vA, vB, #+CCCC | add-int/lit16, etc. |
| 22t | op vA, vB, +CCCC | if-eq, if-ne, if-lt, if-ge, if-gt, if-le |
| 22c | op vA, vB, field/type@CCCC | iget-*, iput-*, instance-of |
| 23x | op vAA, vBB, vCC | aget-*, aput-*, add-*, sub-*, cmp* |
| 30t | op +AAAAAAAA | goto/32 |
| 31i | op vAA, #+BBBBBBBB | const, const-wide/32 |
| 31c | op vAA, string@BBBBBBBB | const-string/jumbo |
| 32x | op vAAAA, vBBBB | move/16 |
| 35c | op {vC..vG}, meth/type@BBBB | invoke-*, filled-new-array |
| 3rc | op {vCCCC..vNNNN}, meth@BBBB | invoke-*/range, filled-new-array/range |
| 51l | op vAA, #+BBBBBBBBBBBBBBBB | const-wide |
| 31t | op vAA, +BBBBBBBB | fill-array-data, packed-switch, sparse-switch |

## DEX Format Support

| Feature | Supported |
|---------|-----------|
| DEX version 035 | Yes |
| DEX version 037 | Yes |
| DEX version 038 | Yes |
| DEX version 039 | Yes |
| Annotation parsing | Yes (type + visibility on class/method) |
| Debug info (line numbers) | Yes |
| Encoded values (all types) | Yes (VALUE_ARRAY, VALUE_ANNOTATION, etc.) |
| Multidex | No |
| Compact DEX (CDEX) | No |
| VDEX container | No |
| OAT files | No |

## Interpreter Features

| Feature | Status |
|---------|--------|
| Exception try/catch/finally | Full support with cross-method unwinding |
| Varargs method invocation | Supported (pack_varargs) |
| Frame pooling | 64-frame pool, zero malloc per call |
| Class hash table | FNV-1a O(1) lookup (4096 buckets) |
| Null-safe type checks | instance-of returns false, check-cast passes |

## Parser Hardening

| Threat | Mitigation |
|--------|------------|
| Path traversal in ZIP entries | Reject `../` in filenames |
| Zip bomb (decompression ratio) | Size limit checks |
| AXML recursion depth | Hard limit on nesting |
| DEX offset validation | Bounds-checked before every table access |
