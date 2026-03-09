# AndroidMini API - DexLoom Framework Stubs

## Overview

DexLoom reimplements a tiny subset of the Android framework as native C functions.
When guest bytecode calls an Android API method, the interpreter dispatches to these
C implementations instead of actual Android framework code.

## Supported Classes

### android.app.Activity

**Purpose**: Entry point for an Android screen. Manages lifecycle and content view.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `<init>()` | `()V` | Allocate Activity object, init fields |
| `onCreate(Bundle)` | `(Landroid/os/Bundle;)V` | Set lifecycle state, call super |
| `setContentView(int)` | `(I)V` | Parse layout resource, build UI tree |
| `findViewById(int)` | `(I)Landroid/view/View;` | Search UI tree for view with matching ID |
| `getResources()` | `()Landroid/content/res/Resources;` | Return Resources singleton |

**Unsupported**: onStart, onResume, onPause, onStop, onDestroy, onSaveInstanceState,
startActivity, finish, getIntent, all other lifecycle methods.

**iOS Mapping**: Activity maps to the RuntimeView screen in the SwiftUI app.

### android.view.View

**Purpose**: Base class for all UI elements.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `setOnClickListener(OnClickListener)` | `(Landroid/view/View$OnClickListener;)V` | Store listener ref on view node |
| `setVisibility(int)` | `(I)V` | Update visibility in UI tree |
| `getId()` | `()I` | Return view ID |

**Unsupported**: setLayoutParams, measure, layout, draw, invalidate, requestLayout,
setBackground, setPadding, all touch/gesture handling beyond click.

**iOS Mapping**: Base protocol for SwiftUI view generation.

### android.widget.TextView

**Purpose**: Displays text.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `<init>(Context)` | `(Landroid/content/Context;)V` | Create text view node |
| `setText(CharSequence)` | `(Ljava/lang/CharSequence;)V` | Update text in UI tree, trigger re-render |
| `getText()` | `()Ljava/lang/CharSequence;` | Return current text |
| `setTextSize(float)` | `(F)V` | Update font size attribute |

**Unsupported**: setTextColor, setTypeface, setGravity, setLines, ellipsize,
all Spannable/Editable methods.

**iOS Mapping**: SwiftUI `Text` view.

### android.widget.Button

**Purpose**: Clickable button (extends TextView).

**Supported Methods**: Inherits all TextView methods plus OnClickListener from View.

**iOS Mapping**: SwiftUI `Button` with text label.

### android.view.ViewGroup

**Purpose**: Container for child views.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `addView(View)` | `(Landroid/view/View;)V` | Append child to UI tree node |
| `removeView(View)` | `(Landroid/view/View;)V` | Remove child from UI tree node |
| `getChildCount()` | `()I` | Return child count |
| `getChildAt(int)` | `(I)Landroid/view/View;` | Return child at index |

**Unsupported**: LayoutParams, margins, all measurement/layout passes.

**iOS Mapping**: SwiftUI container (VStack/HStack).

### android.widget.LinearLayout

**Purpose**: Arranges children in a line.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `<init>(Context)` | `(Landroid/content/Context;)V` | Create layout node |
| `setOrientation(int)` | `(I)V` | Set VERTICAL(1) or HORIZONTAL(0) |

**Unsupported**: gravity, weight, dividers.

**iOS Mapping**: SwiftUI `VStack` (vertical) or `HStack` (horizontal).

### android.content.Context

**Purpose**: Abstract access to app resources and environment.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `getResources()` | `()Landroid/content/res/Resources;` | Return Resources singleton |
| `getString(int)` | `(I)Ljava/lang/String;` | Look up string resource |

**Unsupported**: Everything else (startActivity, getSystemService, getPackageName, etc.).

### android.content.res.Resources

**Purpose**: Access to parsed app resources.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `getString(int)` | `(I)Ljava/lang/String;` | Look up by resource ID |
| `getLayout(int)` | `(I)Landroid/content/res/XmlResourceParser;` | Return layout XML |

**Unsupported**: getDrawable, getColor, getDimension, getInteger, all typed arrays.

### android.os.Bundle

**Purpose**: Key-value map for passing data.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `<init>()` | `()V` | Create empty bundle |

**Unsupported**: All get/put methods (v1 passes null Bundle to onCreate).

### android.view.View.OnClickListener (interface)

**Purpose**: Callback for click events.

**Supported Methods**:
| Method | Signature | Implementation |
|--------|-----------|----------------|
| `onClick(View)` | `(Landroid/view/View;)V` | Dispatch to guest bytecode |

**iOS Mapping**: SwiftUI Button action closure that sends event back to C runtime.
