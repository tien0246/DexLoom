# DexLoom Roadmap

## Milestone Plan

### Milestone 0: Feasibility Analysis -- COMPLETE
- Architecture comparison document
- iOS platform constraint analysis
- Chosen approach: DEX bytecode interpreter + mini framework

### Milestone 1: APK Parsing and Inspection -- COMPLETE
- ZIP file format parser (PKZIP) with hardening (path traversal, zip bomb)
- Entry enumeration and extraction
- Identify classes.dex, AndroidManifest.xml, resources.arsc, res/
- **Delivered**: Full APK content listing and extraction

### Milestone 2: AndroidManifest.xml and resources.arsc Decoding -- COMPLETE
- Android Binary XML (AXML) parser with depth limits
- String pool, resource map, namespace handling
- Manifest parsing: package name, main activity, permissions
- Intent-filter details, meta-data, uses-feature, uses-library, exported flag
- resources.arsc: string pool, type specs, entries, dimension decoding
- **Delivered**: Full manifest and resource decoding

### Milestone 3: DEX Parsing -- COMPLETE
- DEX header validation (magic, checksum, versions 035-039)
- All ID tables: string, type, proto, field, method
- Class definitions with annotations (type + visibility)
- Code items with bytecode, debug info (line number tables)
- Encoded values: VALUE_ARRAY, VALUE_ANNOTATION, all types
- **Delivered**: Full DEX parsing with annotation and debug info support

### Milestone 4: Java Runtime / Class Library -- COMPLETE
- Object model: heap objects with class pointers and fields
- 400+ framework classes spanning Android, Java stdlib, and third-party libraries
- String (35+ methods), HashMap, ArrayList, Collections, Arrays, Objects
- Autoboxing for all primitive wrapper types
- Real ArrayList Iterator with for-each loop support
- Collection interfaces (Iterable/Collection/List/Set/Map) on 15+ classes
- Exception model with cross-method unwinding and finally blocks
- ByteBuffer, WeakReference/SoftReference, Enum, Number, Pair
- **Delivered**: Production-grade class library

### Milestone 5: Bytecode Interpreter -- COMPLETE
- All 256 Dalvik opcodes with edge case handling
- 64-frame pool, FNV-1a class hash table (O(1) lookup)
- Exception try/catch/finally with cross-method unwinding
- Varargs method invocation (pack_varargs)
- Null-safe instance-of/check-cast
- **Delivered**: Full interpreter with production-grade opcode coverage

### Milestone 6: Android UI Rendering -- COMPLETE
- Layout XML parsing (binary XML)
- 30+ view types: TextView, Button, EditText, ImageView, RecyclerView, ListView, GridView, Spinner, SeekBar, RatingBar, RadioButton/Group, FAB, TabLayout, ViewPager, WebView, Chip, BottomNav, SwipeRefreshLayout
- ConstraintLayout basic solver (12 constraint attributes, GeometryReader positioning)
- Drawable loading from APK (PNG/JPEG extraction, UIImage rendering)
- Dimension conversion with padding/margin support
- WebView mapped to WKWebView bridge
- **Delivered**: Rich UI rendering with 30+ view types and constraint solving

### Milestone 7: Activity Lifecycle & Navigation -- COMPLETE
- Full lifecycle: onCreate->onPostCreate->onStart->onResume->onPostResume + teardown
- State save/restore: onSaveInstanceState/onRestoreInstanceState, Activity.recreate()
- Multi-activity navigation with Intent extras
- 16-deep back-stack; startActivityForResult/setResult/finish/onActivityResult
- Fragment lifecycle: onCreateView->onViewCreated->onStart->onResume
- Configuration class
- **Delivered**: Complete activity lifecycle and navigation

### Milestone 8: Event Handling & Touch -- COMPLETE
- onClick on all view types, long-press support
- SwipeRefreshLayout pull-to-refresh
- MotionEvent dispatch
- Menu system: Menu/MenuItem/SubMenu/MenuInflater
- TextWatcher/Editable, CompoundButton isChecked/setChecked/toggle
- Back button: dx_runtime_dispatch_back calls Activity.onBackPressed
- **Delivered**: Full event handling including touch, menus, and text input

### Milestone 9: System Services & I/O -- COMPLETE
- AssetManager.open() extracts from APK; InputStream with real read/available/close
- File I/O: File.createTempFile, Context.openFileInput/openFileOutput
- Filesystem: getExternalFilesDir, Environment paths
- Permissions: checkSelfPermission (safe vs dangerous), requestPermissions with callback
- SQLiteDatabase stub with Cursor and transaction support
- HttpURLConnection stub with simulated 200 response
- BroadcastReceiver: registerReceiver/sendBroadcast with Intent action dispatch
- Service lifecycle: startService->onCreate->onStartCommand; IntentService subclass
- ContentProvider/ContentResolver stub CRUD
- **Delivered**: File I/O, assets, permissions, and system service stubs

### Milestone 10: Advanced Runtime -- COMPLETE
- Reflection: Class.forName, Method.invoke, Field.get/set, getAnnotation
- Advanced reflection: Proxy.newProxyInstance, Array.newInstance, Constructor, getDeclaredMethods/Fields
- JNI bridge: Complete JNIEnv (232 functions), Call*Method, Get/Set*Field, RegisterNatives
- Cooperative threading: Thread.start (synchronous), ExecutorService, Future, CompletableFuture
- LiveData/ViewModel with observer notification
- Third-party libraries: RxJava3 (11 classes, 85 methods), OkHttp3 (18 classes, 120 methods), Retrofit2 (12 classes, 50 methods), Glide (6 classes, 40 methods)
- **Delivered**: Reflection, JNI, threading, and major third-party library support

### Milestone 11: Debug & Diagnostics -- COMPLETE
- UI tree inspector (visual hierarchy debugging)
- Heap inspector (memory/object analysis)
- Error diagnostics (enhanced error reporting)
- Build/VERSION constants: SDK_INT=33, RELEASE="13"
- Line number tables from DEX debug_info_item
- **Delivered**: Debug tooling for runtime inspection

## Current Feature Summary

### Fully Supported
- All 256 Dalvik opcodes with edge case handling
- 400+ framework classes (Android, Java, Kotlin, RxJava, OkHttp, Retrofit, Glide)
- 30+ view types with ConstraintLayout basic solver
- Full activity lifecycle with state save/restore and 16-deep back-stack
- Fragment lifecycle, Service, BroadcastReceiver, ContentProvider
- Reflection including Proxy, Constructor, annotations
- JNI bridge (232 functions)
- AssetManager, File I/O, permissions system
- Touch events, menus, text input, long-press
- Cooperative threading, LiveData/ViewModel
- Debug tools: UI tree inspector, heap inspector, error diagnostics

### Known Limitations
- Compose apps: fundamentally unsupported (need Compose compiler runtime)
- JNI: Can't load .so files (no dlopen); provides env for DEX-side JNI calls only
- HttpURLConnection: stub only, no real networking yet
- Threading: cooperative (synchronous) only, no true concurrency
- Multidex: not supported
- Obfuscated/heavily optimized APKs: may have issues
