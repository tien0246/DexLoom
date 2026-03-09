# DexLoom Roadmap

## Milestone Plan

### Milestone 0: Feasibility Analysis (COMPLETE)
- Architecture comparison document
- iOS platform constraint analysis
- Chosen approach: DEX bytecode interpreter + mini framework

### Milestone 1: APK Parsing and Inspection
- ZIP file format parser (PKZIP)
- Entry enumeration and extraction
- Identify classes.dex, AndroidManifest.xml, resources.arsc, res/
- CLI inspector tool for validation
- **Deliverable**: Can list and extract all APK contents

### Milestone 2: AndroidManifest.xml and resources.arsc Decoding
- Android Binary XML (AXML) parser
- String pool, resource map, namespace handling
- Extract package name, main activity, permissions
- resources.arsc: string pool, type specs, entries
- **Deliverable**: Can print manifest contents and resource strings

### Milestone 3: DEX Parsing
- DEX header validation (magic, checksum, version)
- String ID table, Type ID table, Proto ID table
- Field ID table, Method ID table
- Class definitions
- Code items with bytecode
- **Deliverable**: Can dump all classes, methods, and disassemble bytecode

### Milestone 4: Tiny Java Runtime / Class Library Subset
- Object model: heap objects with class pointers and fields
- String representation (backed by C strings or UTF-16)
- Array types (object arrays, int arrays)
- Basic exception model (try/catch flow)
- Class metadata: vtables, field layouts, static fields
- **Deliverable**: Can create objects, call methods, access fields in unit tests

### Milestone 5: Bytecode Interpreter
- Register file per frame
- Opcode dispatch loop (switch-based)
- Supported opcodes for demo:
  - const/4, const/16, const-string
  - move, move-result
  - return-void, return
  - invoke-virtual, invoke-direct, invoke-static
  - iget, iput, sget, sput
  - new-instance
  - if-eq, if-ne, goto
- Method call/return with frame push/pop
- **Deliverable**: Can execute simple methods in unit tests

### Milestone 6: Android UI Subset Mapped to SwiftUI
- Layout XML parsing (binary XML subset)
- UI tree: DxNode with type, attributes, children
- Layout engine: LinearLayout vertical/horizontal
- View types: TextView, Button, LinearLayout
- Render model: serialized tree for Swift consumption
- SwiftUI bridge: recursive view builder from render model
- **Deliverable**: Can render a static layout from XML

### Milestone 7: Event Loop and Activity Lifecycle
- Activity.onCreate(Bundle) simulation
- setContentView(layoutId) -> parse layout, build UI tree
- findViewById(id) -> return view reference
- OnClickListener dispatch
- setText() -> UI tree update -> SwiftUI re-render
- **Deliverable**: Can run Activity lifecycle and handle clicks

### Milestone 8: Demo APK Execution
- End-to-end: load APK -> parse -> interpret -> render -> interact
- Demo app: Hello World with button that changes text
- Logging and trace output
- **Deliverable**: Working demo on iOS simulator

### Milestone 9: Debugging, Logging, and Conformance Tests
- Trace mode: log every opcode executed
- Method entry/exit logging
- UI tree dump
- Regression test suite
- Known-good test APKs
- **Deliverable**: Debuggable, testable runtime

## Version 1 Feature Matrix

### Supported
- One Activity per app
- onCreate lifecycle callback
- setContentView with layout resource
- findViewById
- TextView: setText, getText
- Button: setOnClickListener
- LinearLayout: vertical orientation
- String resources
- Layout resources (binary XML)
- ~25 DEX opcodes (enough for demo)

### Explicitly Not Supported in v1
- Services, BroadcastReceivers, ContentProviders
- Multiple Activities / Intents
- JNI / native code
- Threading / AsyncTask / coroutines
- Networking
- File I/O from guest code
- Fragments
- RecyclerView, ImageView, EditText
- Animations
- Permissions system
- SharedPreferences
- Reflection
- Multidex
- Kotlin-specific bytecode patterns
- Obfuscated/optimized APKs
