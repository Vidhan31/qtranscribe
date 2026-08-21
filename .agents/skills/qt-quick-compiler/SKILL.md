---
name: qt-quick-compiler
description: >-
  Expert guide and auditing workflow for Qt Quick Compiler and Ahead-Of-Time (AOT)
  compilation (qmlcachegen, qmltc). Use when optimizing QML code for 100% native
  C++ binding generation, resolving .aotstats compilation failures, eliminating JavaScript
  bytecode fallbacks, enforcing strict static typing, ComponentBehavior: Bound scoping,
  and declarative C++ integration.
license: MIT
compatibility: Designed for Claude Code, Antigravity, and AI coding agents.
disable-model-invocation: false
metadata:
  author: qtranscribe
  version: "1.1"
  qt-version: "6.5+ (Qt 6.8 / Qt 6.11)"
  category: optimization
---

# Qt Quick Compiler & AOT Optimization Guide

This skill provides rules, modern pragmas, patterns, and an automated audit workflow for achieving 100% native C++ compilation of QML bindings using Qt 6's Ahead-Of-Time (AOT) compiler (`qmlcachegen`) and the Qt Quick Compiler toolchain.

---

## 1. Compiler Architecture Quick Reference

In modern Qt 6 (Qt 6.5+ through Qt 6.11), the open-source Qt Quick Compiler toolchain consists of:

| Tool | License | Description | Output |
| :--- | :--- | :--- | :--- |
| **`qmlcachegen`** | Open Source (LGPL/GPL/Commercial) | Standard Qt 6 AOT script compiler. Automatically compiles type-safe QML bindings and functions into native C++ functions. | `.rcc/qmlcache/*_qml.cpp` and `.aotstats` |
| **`qmltc`** | Open Source (LGPL/GPL/Commercial) | QML Type Compiler. Generates standalone C++ classes for QML document types (opt-in via `ENABLE_TYPE_COMPILER`). | `*qml_qmltc.h/.cpp` |

`qmlcachegen` runs automatically during `cmake --build` for any target defined with `qt_add_qml_module()`. When a binding cannot be statically resolved or involves dynamic JavaScript types, `qmlcachegen` falls back to interpreted byte-code (`codegenResult != 0`) and logs the reason into a corresponding `.aotstats` file.

---

## 2. Essential Modern Pragmas (Qt 6.5 - Qt 6.11)

Place relevant pragmas at line 1 of `.qml` files to configure compiler behavior:

### `pragma ComponentBehavior: Bound` (Qt 6.4+, Recommended for all QML)
```qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
```
- **Why**: Eliminates dynamic context scopes for inner components (e.g. delegates in `ListView`, `Repeater`, `TableView`). IDs in lexical scopes and model data via `required property` are statically resolvable at compile time.

### `pragma ValueTypeBehavior: Addressable, Assertable` (Qt 6.6+ / Qt 6.8+)
```qml
pragma ValueTypeBehavior: Addressable
pragma ValueTypeBehavior: Assertable
```
- **Why**: Enables lowercase value type names (`rect`, `size`, `point`, `color`, `font`) to be used with the `as` and `instanceof` operators (e.g. `(modelData as rect).x`), giving `qmlcachegen` full type information for value types. `Assertable` (Qt 6.8+) guarantees strict type assertion without accidental value type construction.

### `pragma FunctionSignatureBehavior: Enforced` (Qt 6.5+, Default in Qt 6.7+)
- **Why**: Enforces type annotations on JavaScript functions (`function add(a: int, b: int): int`), enabling `qmlcachegen` to compile functions to native C++ methods rather than falling back to `QJSValue`.

### `pragma ListPropertyAssignBehavior: ReplaceIfNotDefault` (Qt 6.3+)
```qml
pragma ListPropertyAssignBehavior: ReplaceIfNotDefault
```
- **Why**: Specifies that assigning to a non-default list property replaces rather than appends to inherited lists, creating predictable list initialization.

---

## 3. Mandatory Rules for 100% Native AOT Compilation

### Rule 1: Always Declare Bound Component Behavior
Add `pragma ComponentBehavior: Bound` at line 1 of every `.qml` document.
- **Why**: By default, components in QML have unbound lookup scopes. `Bound` guarantees that outer lexical scopes are statically resolvable at compile time and delegates require explicit typed properties.

---

### Rule 2: Eliminate Dynamic Color Helper Calls in Reactive Bindings
Never call global `Qt.rgba()`, `Qt.lighter()`, `Qt.darker()`, or `Qt.tint()` directly inside property bindings.
- **Problem**: Global `Qt.*` color functions return dynamic `QJSValue`/`QVariant` types via runtime lookups. `qmlcachegen` cannot prove the return type statically and falls back to interpreted bytecode (`codegenResult: 2`: *"Cannot generate efficient code for internal conversion with incompatible or ambiguous types"*).
- **Note**: `Qt.alpha()` does **not** exist in standard `QtQuick`/`QtQml`. Wrapping `Qt.*` methods in QML JavaScript functions still causes bytecode fallback.
- **Solutions**:
  1. **Static Colors**: Use 8-digit hex constants with alpha channels: `"#80ffffff"`, `"#26000000"`.
  2. **Dynamic / Theme Colors**: Expose a C++ utility class or singleton with typed `QColor` methods registered via `QML_ELEMENT` / `QML_SINGLETON`:
  ```cpp
  // ColorUtils.h (C++ Backend)
  class ColorUtils : public QObject {
      Q_OBJECT
      QML_ELEMENT
      QML_SINGLETON
  public:
      Q_INVOKABLE static QColor withAlpha(const QColor &baseColor, qreal alpha) {
          QColor c = baseColor;
          c.setAlphaF(std::clamp(alpha, 0.0, 1.0));
          return c;
      }
      Q_INVOKABLE static QColor tint(const QColor &baseColor, const QColor &tintColor) {
          // Fast C++ color tinting / alpha blending
          qreal a = tintColor.alphaF();
          return QColor::fromRgbF(
              baseColor.redF() * (1.0 - a) + tintColor.redF() * a,
              baseColor.greenF() * (1.0 - a) + tintColor.greenF() * a,
              baseColor.blueF() * (1.0 - a) + tintColor.blueF() * a,
              baseColor.alphaF()
          );
      }
  };
  ```
  ```qml
  // Usage in QML:
  Rectangle {
      color: ColorUtils.withAlpha(palette.windowText, 0.15)
  }
  ```

---

### Rule 3: Replace Untyped JS Arrays with Typed Sequences, `ListModel`, or `QAbstractListModel`
Never use `readonly property var items = [...]` as a model for `ListView`, `Repeater`, or `ComboBox`.
- **Problem**: `var` is untyped (`QVariant`), forcing delegates to perform dynamic runtime lookups on `QJSValue`.
- **Solutions**:
  1. **Typed Sequences (`list<T>`)**:
     ```qml
     readonly property list<string> categories: ["Audio", "Network", "Advanced"]

     ListView {
         model: root.categories
         delegate: ItemDelegate {
             required property string modelData
             required property int index
             text: modelData
         }
     }
     ```
  2. **`ListModel` with `ListElement`**:
     ```qml
     ListModel {
         id: categoryModel
         ListElement { categoryId: 0; title: "Audio"; description: "Microphone configuration" }
         ListElement { categoryId: 1; title: "Network"; description: "API endpoints" }
     }

     ListView {
         model: categoryModel
         delegate: ItemDelegate {
             id: delegateItem
             required property int categoryId
             required property string title
             required property string description
             required property int index

             text: delegateItem.title
         }
     }
     ```
  3. **C++ `QAbstractListModel`**: Expose strongly-typed C++ models registered via `QML_ELEMENT`.

---

### Rule 4: Explicitly Type All Delegate Roles and Properties
Every item delegate MUST declare `required property <Type> <roleName>` for every model role and `required property int index`.
- **Why**: When `pragma ComponentBehavior: Bound` is active, context properties like `model.title` are prohibited. Explicit `required property` declarations allow `qmlcachegen` to generate direct C++ property reads on the delegate item.

---

### Rule 5: Scope Nested Delegate Property Lookups
Inside a delegate's child hierarchy, always qualify delegate properties and `index` with the delegate's `id`:
```qml
Repeater {
    model: 5
    delegate: Item {
        id: barDelegate
        required property int index

        Rectangle {
            // Qualify with barDelegate.index, NOT bare 'index'
            height: 10 + (barDelegate.index * 4)
        }
    }
}
```

---

### Rule 6: ScrollView and TextArea Child Composition
In Qt Quick Controls, do NOT bind `contentItem: TextArea { ... }` on a `ScrollView`.
- **Problem**: `ScrollView` expects a `Flickable` as its `contentItem`. `TextArea` is a `Control`. Overriding `contentItem` breaks type contracts and scrolling behavior.
- **Solution**: Place `TextArea` as a direct child of `ScrollView`:
```qml
ScrollView {
    id: textScrollView
    width: root.width
    height: 120
    clip: true

    TextArea {
        id: textInput
        wrapMode: TextArea.Wrap
        selectByMouse: true
        font.pixelSize: 12
        color: textInput.palette.windowText
    }
}
```

---

### Rule 7: Dialog and Popup Positioning and Palette Access
`Dialog` and `Popup` inherit `QtQuick.Controls.Popup` (`QObject`, not `Item`), and live in the window's `Overlay`.
- Do NOT use `anchors.centerIn: parent` on `Dialog` or `Popup`.
- Use explicit sizing (`width`, `height`, `x`, `y`) or `anchors.centerIn: Overlay.overlay`.
- Resolve system palette colors inside popups using `SystemPalette { id: sysPal; colorGroup: SystemPalette.Active }` or the component's own palette.

---

### Rule 8: Avoid Inline Dynamic JavaScript in Bindings
Avoid calling JavaScript string/regex manipulation methods (`.trim()`, `.split(/\s+/)`, dynamic object mutations) inside reactive property bindings.
- **Problem**: `qmlcachegen` cannot compile arbitrary JS runtime constructs into C++, forcing bytecode fallback.
- **Solution**: Perform string parsing and formatting in C++ backend setters or upon explicit user actions (e.g. `onClicked`).

---

## 4. Auditing & Metric Verification Workflow

To inspect and measure Ahead-Of-Time C++ compilation for all QML components:

### Method A: Built-in Qt CMake Target (Qt 6.8+ / Qt 6.11)
Run the standard CMake target in your build directory:
```bash
cmake --build build --target all_aotstats
```

---

### Method B: Inspecting Specific Uncompiled Bindings
To view the line numbers and exact reasons for uncompiled bindings in a specific file:

```bash
python3 -c '
import json, sys
with open(sys.argv[1]) as f:
    d = json.load(f)
    for mod in d.get("modules", []):
        for mf in mod.get("moduleFiles", []):
            for e in mf.get("entries", []):
                if e.get("codegenResult") != 0:
                    print(f"Line {e.get(\"line\")}:{e.get(\"column\")} [{e.get(\"functionName\")}] -> {e.get(\"message\")}")
' build/src/ui/.rcc/qmlcache/<Filename>_qml.cpp.aotstats
```
