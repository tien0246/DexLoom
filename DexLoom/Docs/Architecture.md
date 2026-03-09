# DexLoom Architecture Decision Document

## Chosen Architecture

**Approach: DEX Bytecode Interpreter + Minimal Android Compatibility Layer**

A pure interpreter for Dalvik bytecode running inside a normal iOS app process,
combined with a tiny reimplementation of essential Android framework classes,
and a bridge that maps Android UI concepts to SwiftUI.

## Architecture Comparison

### 1. Full Android Emulation (QEMU/system-level)

| Dimension          | Assessment |
|--------------------|-----------|
| Feasibility        | IMPOSSIBLE on jailed iOS. Requires RWX memory for TCG, kernel-level access, or JIT. iOS forbids all of these in user-space apps. |
| Legal              | Would violate App Store rules (code execution, emulation restrictions). |
| Performance        | Extremely poor even if possible. |
| Complexity         | Enormous - full Linux kernel, Android userspace, graphics stack. |
| Maintainability    | Unmaintainable for a small team. |
| Min demo scope     | Years of work. |
| Blockers           | Fundamental iOS platform restrictions. |
| **Verdict**        | **REJECTED. Impossible on jailed iOS.** |

### 2. ART/Dalvik Bytecode Interpreter Inside iOS App

| Dimension          | Assessment |
|--------------------|-----------|
| Feasibility        | FEASIBLE. Pure interpretation requires no JIT/RWX memory. DEX bytecode is well-documented. A register-based interpreter can run in pure C. |
| Legal              | Interpreting data (bytecode) is not code generation. This is how game scripting engines (Lua, etc.) work on iOS and pass App Store review. |
| Performance        | Slow compared to native, but adequate for simple demo apps. 100-1000x slower than compiled code, but a Hello World app is I/O bound, not CPU bound. |
| Complexity         | Medium. ~50-100 opcodes for a minimal subset. Well-understood problem space. |
| Maintainability    | Good. Interpreter is a single dispatch loop. Opcodes are independent. |
| Min demo scope     | ~20 opcodes for a button-click demo. |
| Blockers           | None fundamental. Requires reimplementing Android framework stubs. |
| **Verdict**        | **CHOSEN. Best feasibility/complexity ratio.** |

### 3. APK-to-IR Translator (AOT transpilation)

| Dimension          | Assessment |
|--------------------|-----------|
| Feasibility        | Feasible but over-engineered for v1. Would require designing a custom IR, writing a DEX-to-IR compiler, and an IR interpreter. Two interpretation layers. |
| Legal              | Fine - no code generation, just data transformation. |
| Performance        | Marginally better than direct interpretation if IR is optimized. |
| Complexity          | High. Two parsers, one compiler, one interpreter. |
| Maintainability    | Harder to debug - two abstraction layers between source and execution. |
| Min demo scope     | Same as approach 2, but with more infrastructure. |
| Blockers           | None, but unnecessary complexity. |
| **Verdict**        | **DEFERRED. Good for v2 optimization, overkill for v1.** |

### 4. APK Resource + Manifest Parser with Custom Mini-Runtime

| Dimension          | Assessment |
|--------------------|-----------|
| Feasibility        | This IS part of approach 2. Parsing APK/manifest/resources is a prerequisite layer. Not a standalone approach. |
| Legal              | Fine. |
| Performance        | N/A - not a complete approach. |
| Complexity         | Low for parsing alone, but doesn't execute anything. |
| Maintainability    | Good. |
| Min demo scope     | Can display parsed info but not run apps. |
| Blockers           | Doesn't actually run bytecode. |
| **Verdict**        | **SUBSUMED into approach 2 as Milestones 1-2.** |

### 5. Restricted Android Compatibility Layer (Java API stubs only)

| Dimension          | Assessment |
|--------------------|-----------|
| Feasibility        | This is also part of approach 2. The framework stubs (Activity, View, etc.) need an execution engine underneath. |
| Legal              | Fine. Reimplementing an API surface is legal. |
| Performance        | N/A - needs an executor. |
| Complexity         | Medium. Each Android class needs a careful stub. |
| Maintainability    | Good if kept minimal. |
| Min demo scope     | ~10 classes for Hello World. |
| Blockers           | Useless without bytecode execution. |
| **Verdict**        | **SUBSUMED into approach 2 as Milestone 4.** |

### 6. AOT Translation to Safe Interpreted Form

| Dimension          | Assessment |
|--------------------|-----------|
| Feasibility        | Feasible. Could translate DEX to a custom bytecode at import time. But gains are marginal over interpreting DEX directly. |
| Legal              | Fine. |
| Performance        | Could be slightly faster with a stack-based or threaded dispatch form. |
| Complexity         | Medium-high. Need a translator AND an interpreter. |
| Maintainability    | Medium. |
| Min demo scope     | Same as approach 2 with extra build step. |
| Blockers           | None. |
| **Verdict**        | **DEFERRED to v2. Direct DEX interpretation is simpler for v1.** |

## Why This Architecture Wins

1. **Proven pattern**: Lua, JavaScript Core (interpreter mode), WASM interpreters all ship on iOS App Store. Interpreting structured bytecode inside an app is explicitly allowed.
2. **DEX is well-specified**: The Dalvik bytecode format has excellent public documentation. ~256 opcodes, register-based, simple type system.
3. **Minimal viable demo requires ~20 opcodes**: const, move, invoke-virtual, invoke-direct, return, iput, iget, new-instance, plus a few more.
4. **Clean separation of concerns**: Parser, interpreter, framework stubs, and UI bridge are independent layers.
5. **No platform violations**: Pure data interpretation. No RWX memory, no JIT, no unsigned code, no private APIs.

## System Design

```
+------------------+
|   SwiftUI App    |  (iOS app shell, file picker, runtime view)
+------------------+
        |
+------------------+
|   Swift Bridge   |  (calls C functions, receives UI tree updates)
+------------------+
        |
+------------------+
|  C Runtime Core  |
|  +-------------+ |
|  | APK Parser  | |  -> ZIP extraction, AXML decode, resource parse
|  +-------------+ |
|  | DEX Parser  | |  -> header, tables, code items, opcodes
|  +-------------+ |
|  | Interpreter | |  -> register-based dispatch loop
|  +-------------+ |
|  | Mini Java   | |  -> Object, String, arrays, exceptions
|  +-------------+ |
|  | Mini Android| |  -> Activity, View, TextView, Button stubs
|  +-------------+ |
|  | UI Bridge   | |  -> abstract UI tree, layout engine, events
|  +-------------+ |
+------------------+
```

## Execution Flow

1. User selects APK file via iOS document picker
2. APK copied to app sandbox
3. C runtime: parse ZIP, extract classes.dex, AndroidManifest.xml, resources
4. C runtime: parse DEX, build class/method tables
5. C runtime: find main Activity class from manifest
6. C runtime: simulate Activity.onCreate() -> interpret bytecode
7. Bytecode calls setContentView() -> triggers layout XML parsing
8. Layout XML parsed -> UI tree built in C
9. UI tree serialized to render model -> passed to Swift bridge
10. SwiftUI renders the UI tree
11. User taps button -> event sent back to C runtime
12. C runtime: dispatch OnClickListener.onClick() -> interpret bytecode
13. Bytecode calls TextView.setText() -> UI tree updated
14. Updated render model -> SwiftUI re-renders
