# AndroidMini API - DexLoom Framework Stubs

## Overview

DexLoom reimplements **400+ Android/Java/third-party classes** as native C functions.
When guest bytecode calls an API method, the interpreter dispatches to these C implementations
instead of actual framework code. Classes span the Android SDK, Java standard library,
Kotlin stdlib, and popular third-party libraries.

## Class Categories

### android.app (Activity, Service, Fragment, Application)
- **Activity**: Full lifecycle (onCreate through onDestroy), setContentView, findViewById, startActivity/startActivityForResult, finish, onBackPressed, onSaveInstanceState/onRestoreInstanceState, recreate(), onCreateOptionsMenu/onOptionsItemSelected, onActivityResult
- **Fragment**: onCreateView, onViewCreated, onStart, onResume lifecycle
- **Service**: startService -> onCreate -> onStartCommand; IntentService subclass
- **Application**: onCreate, getApplicationContext

### android.content (Context, Intent, BroadcastReceiver, ContentProvider)
- **Context**: getResources, getString, getSystemService, checkSelfPermission (safe vs dangerous), requestPermissions, getApplicationInfo, getClassLoader, bindService, openFileInput/openFileOutput, getExternalFilesDir
- **Intent**: Constructor with action/class, putExtra/getExtra for all types, setAction, setData
- **BroadcastReceiver**: registerReceiver/sendBroadcast with Intent action dispatch
- **ContentProvider/ContentResolver**: Stub CRUD (query/insert/update/delete)
- **SharedPreferences**: getString/putString/getInt/putInt with apply/commit

### android.view (View, ViewGroup, Menu, MotionEvent)
- **View**: setOnClickListener (all view types), setOnLongClickListener, setVisibility, getId, setTag/getTag, setPadding, setBackground, invalidate
- **ViewGroup**: addView, removeView, getChildCount, getChildAt
- **Menu/MenuItem/SubMenu/MenuInflater**: Full menu system
- **MotionEvent**: Touch event dispatch with action types

### android.widget (30+ view types)
- **Text**: TextView, EditText (with SwiftUI TextField binding), AutoCompleteTextView
- **Buttons**: Button, ImageButton, ToggleButton, FloatingActionButton
- **Compound**: CheckBox, Switch, RadioButton, RadioGroup (isChecked/setChecked/toggle)
- **Lists**: RecyclerView (adapter pattern), ListView, GridView, Spinner
- **Input**: SeekBar, RatingBar, SearchView
- **Display**: ImageView (with drawable loading from APK), ProgressBar, WebView (WKWebView bridge)
- **Layout**: LinearLayout, RelativeLayout, FrameLayout, ConstraintLayout (basic solver), ScrollView, NestedScrollView, CoordinatorLayout, SwipeRefreshLayout
- **Navigation**: TabLayout, ViewPager, BottomNavigationView, Toolbar, DrawerLayout
- **Material**: Chip, ChipGroup, CardView, Snackbar, TextInputLayout

### android.os
- **Bundle**: Full get/put for all types
- **Handler/Looper**: Post and delayed message dispatch
- **Build/VERSION**: SDK_INT=33, RELEASE="13", MANUFACTURER/MODEL/DEVICE
- **Environment**: getExternalStorageDirectory and related paths

### android.content.res
- **Resources**: getString, getLayout, getDimension, getDrawable, getColor
- **AssetManager**: open() extracts files from APK; returns InputStream with real read/available/close
- **Configuration**: Device configuration data

### android.animation
- ViewPropertyAnimator, ValueAnimator, ObjectAnimator, AnimatorSet stubs

### androidx / Jetpack
- **LiveData/MutableLiveData**: setValue notifies observers, observe with lifecycle
- **ViewModel/ViewModelProvider**: ViewModelProvider.get instantiates ViewModel subclass
- **RecyclerView**: Full adapter pattern with ViewHolder

### java.lang
- **Object**: equals, hashCode, toString, getClass (returns Class object), clone, notify/wait
- **String**: 35+ methods (substring, indexOf, replace, replaceAll, split, trim, toLowerCase, toUpperCase, format, valueOf, join, getBytes, intern, matches, equalsIgnoreCase, codePointAt, etc.)
- **Class**: forName, getName, isInterface, getSuperclass, isArray, getAnnotation, getDeclaredMethods, getDeclaredFields
- **Thread**: currentThread, start (cooperative/synchronous), getName, setName, sleep
- **Enum**: name, ordinal, compareTo, values, valueOf
- **Number/Math**: Integer, Long, Float, Double, Boolean, Byte, Short, Character with valueOf/parse/unboxing
- **Throwable/Exception**: getMessage, toString, getCause, printStackTrace, getStackTrace

### java.lang.reflect
- **Method**: invoke with real dispatch, getName, getParameterTypes
- **Field**: get/set with real field access, getName
- **Constructor**: newInstance
- **Proxy**: newProxyInstance with InvocationHandler dispatch
- **Array**: newInstance, get, set, getLength

### java.util
- **ArrayList**: Full implementation with real Iterator (hasNext/next for for-each loops)
- **HashMap**: get/put/remove/containsKey/containsValue/putAll/getOrDefault/putIfAbsent/toString; keySet/values/entrySet return iterable collections
- **HashSet, LinkedHashMap, TreeMap**: Basic operations
- **Collections**: emptyList/emptyMap, singleton/singletonMap, addAll, sort, unmodifiableList
- **Arrays**: asList, copyOf, fill, equals, toString
- **Objects**: equals, hashCode, requireNonNull, toString

### java.io
- **File**: createTempFile, exists, getName, getAbsolutePath, length, delete, mkdir, listFiles
- **InputStream/OutputStream**: Real read/write/close with byte buffer backing
- **BufferedReader/InputStreamReader**: readLine, read, close

### java.nio
- **ByteBuffer**: Field-backed storage with position/limit/capacity, get/put, multi-byte ops, byte order

### java.lang.ref
- **WeakReference/SoftReference**: Extend Reference; get/clear/enqueue
- **ReferenceQueue**: Stub

### java.util.concurrent
- **ExecutorService**: submit/execute (cooperative)
- **Future**: get (returns immediately)
- **CompletableFuture**: thenApply/thenAccept/thenCompose stubs

### Third-Party Libraries

#### RxJava3 (11 classes, 85 methods)
- Observable, Single, Completable, Maybe, Flowable
- Operators: map, flatMap, filter, subscribeOn, observeOn, subscribe
- Disposable, CompositeDisposable, Schedulers

#### OkHttp3 (18 classes, 120 methods)
- OkHttpClient, Request, Request.Builder, Response, Response.Builder
- Call, Callback, Interceptor, MediaType, RequestBody, ResponseBody
- Headers, HttpUrl, Cache, ConnectionPool, Dispatcher

#### Retrofit2 (12 classes, 50 methods)
- Retrofit, Retrofit.Builder, Call, Callback, Response
- Converter, GsonConverterFactory, RxJava3CallAdapterFactory
- HTTP method annotations (@GET, @POST, @PUT, @DELETE)

#### Glide (6 classes, 40 methods)
- Glide, RequestManager, RequestBuilder, RequestOptions
- Target, DrawableTransitionOptions

### Utility
- **android.util.Log**: d/i/w/e/v with tag+message logging
- **android.util.Pair**: Field-backed first/second, create() factory
- **ClassLoader**: loadClass, getParent, getResource
