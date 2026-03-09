# DEX Support Matrix - Version 1

## Supported Opcodes

| Opcode | Hex  | Format | Description |
|--------|------|--------|-------------|
| nop | 0x00 | 10x | No operation |
| move | 0x01 | 12x | Move register |
| move-result | 0x0A | 11x | Move result of invoke |
| move-result-object | 0x0C | 11x | Move object result |
| move-exception | 0x0D | 11x | Move exception to register |
| return-void | 0x0E | 10x | Return void |
| return | 0x0F | 11x | Return value |
| return-object | 0x11 | 11x | Return object |
| const/4 | 0x12 | 11n | Const 4-bit |
| const/16 | 0x13 | 21s | Const 16-bit |
| const | 0x14 | 31i | Const 32-bit |
| const-string | 0x1A | 21c | Load string ref |
| new-instance | 0x22 | 21c | Allocate object |
| check-cast | 0x1F | 21c | Type check |
| iget | 0x52 | 22c | Instance field get (int) |
| iget-object | 0x54 | 22c | Instance field get (object) |
| iput | 0x59 | 22c | Instance field put (int) |
| iput-object | 0x5B | 22c | Instance field put (object) |
| sget-object | 0x62 | 21c | Static field get (object) |
| sput-object | 0x69 | 21c | Static field put (object) |
| invoke-virtual | 0x6E | 35c | Virtual method call |
| invoke-direct | 0x70 | 35c | Direct method call |
| invoke-static | 0x71 | 35c | Static method call |
| invoke-interface | 0x72 | 35c | Interface method call |
| if-eq | 0x32 | 22t | Branch if equal |
| if-ne | 0x33 | 22t | Branch if not equal |
| if-eqz | 0x38 | 21t | Branch if zero |
| if-nez | 0x39 | 21t | Branch if not zero |
| goto | 0x28 | 10t | Unconditional branch |

## Unsupported Opcodes (v1)

All array opcodes (aget, aput, array-length, new-array, fill-array)
All arithmetic/logic opcodes except what's needed for demo
All comparison opcodes (cmp*)
All conversion opcodes (int-to-*, etc.)
packed-switch, sparse-switch
filled-new-array
monitor-enter, monitor-exit
throw
invoke-virtual/range and other /range variants
All wide (64-bit) operations

## Unsupported Opcode Handling

When the interpreter encounters an unsupported opcode:
1. Log: "UNSUPPORTED OPCODE 0x%02x at pc=%d in %s.%s"
2. Set VM error state
3. Return DX_ERR_UNSUPPORTED_OPCODE
4. Caller can inspect error and abort gracefully

## DEX Format Support

| Feature | Supported |
|---------|-----------|
| DEX version 035 | Yes |
| DEX version 037 | Yes |
| DEX version 038 | Yes |
| DEX version 039 | Partial |
| Multidex | No |
| Compact DEX (CDEX) | No |
| VDEX container | No |
| OAT files | No |

## Instruction Formats Supported

| Format | Description | Used By |
|--------|-------------|---------|
| 10x | op | nop, return-void |
| 10t | op +AA | goto |
| 11n | op vA, #+B | const/4 |
| 11x | op vAA | move-result, return |
| 12x | op vA, vB | move |
| 21s | op vAA, #+BBBB | const/16 |
| 21t | op vAA, +BBBB | if-eqz, if-nez |
| 21c | op vAA, type/string@BBBB | const-string, new-instance, check-cast |
| 22c | op vA, vB, field@CCCC | iget, iput |
| 22t | op vA, vB, +CCCC | if-eq, if-ne |
| 31i | op vAA, #+BBBBBBBB | const |
| 35c | op {vC,vD,vE,vF,vG}, meth@BBBB | invoke-* |
