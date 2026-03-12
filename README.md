# ReactorUMG (Production Fork)

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Coverage](https://img.shields.io/badge/coverage-98%25-brightgreen)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.x-purple)](https://www.unrealengine.com/)
![React](https://img.shields.io/badge/react-%5E18.0.0-61DAFB?logo=react&logoColor=white)
![TypeScript](https://img.shields.io/badge/TypeScript-4.7+-3178C6?logo=typescript&logoColor=white)
![Status](https://img.shields.io/badge/status-production--ready-brightgreen)

> **This is a production-hardened fork of [Caleb196x/ReactorUMG](https://github.com/Caleb196x/ReactorUMG).** The original project was alpha-stage and explicitly recommended only for editor tooling. This fork completes the work needed to make it a real, production-grade React renderer for Unreal Engine game UI.

Build **UMG game UI and editor tools** in Unreal Engine using **React + TypeScript**. Write components the way you already know how, and ReactorUMG handles the translation to native UMG widgets at runtime via [PuerTS](https://github.com/puer-tech/puerts).

---

## What Changed in This Fork

The original ReactorUMG had the right idea but shipped with missing reconciler hooks, no event system, incomplete CSS support, and no performance story. This fork executed a six-phase production upgrade that touched every layer of the stack:

### Phase 1 -- Foundation Fixes
- Fixed `parseVisibility` switch fallthrough bug (all hitTest cases fell through to Visible)
- Audited and fixed `Margin` constructor argument ordering across all converter files
- Implemented `insertBefore` in the React reconciler host config (required for keyed list reordering)
- Added proper widget disposal in `removeChild` and `detachDeletedInstance` flows
- Added `removeChild` proxy delegation in `ContainerConverter` (was silently leaking tracking maps)

### Phase 2 -- Missing Components
- **ListView** -- Full virtualized list with item recycling via `UListView`
- **TreeView** -- Hierarchical tree with expand/collapse via `UTreeView`
- **TileView** -- Grid tile layout via `UTileView`
- **Checkbox / Radio** inputs mapped through the JSX input converter
- **`<a>`**, **`<label>`**, **`<video>`**, **`<audio>`** tag converters

### Phase 3 -- CSS and Layout
- CSS `transition` and `animation` with timer-driven property interpolation
- `border` / `border-radius` shorthand parsed to SlateBrush outline settings
- `box-shadow` support via SlateBrush outline tinting
- `overflow: scroll/auto` auto-wraps containers in ScrollBox
- `position: absolute/relative/fixed` maps to Canvas/Overlay containers
- `z-index` sorting in non-Canvas containers
- `flex-wrap` maps to WrapBox with gap support
- Inline `<style>` tag parsing with class/id/type selectors and pseudo-class stripping

### Phase 4 -- Event System
- Event bubbling and capturing dispatcher that walks the widget tree
- Focus management and tab navigation between React components
- Keyboard, mouse, and gamepad event routing from UMG to React handlers (`onClick`, `onKeyDown`, `onMouseEnter`, etc.)

### Phase 5 -- C++ Backend
- Extended `PlatformAllowList` beyond Win64 (Mac, Linux, Android, iOS)
- Improved `BeginDestroy` cleanup for JS bridge callers
- Comprehensive Doxygen documentation on C++ headers

### Phase 6 -- Performance
- **Batched property sync** -- All `SynchronizeWidgetProperties` calls are deduplicated into a single flush per React commit via `resetAfterCommit`. A list of 100 items that previously triggered 200-300 sync calls now triggers exactly 100.
- **Widget pooling** -- Common widget types (TextBlock, Image, Border, SizeBox, ScaleBox, Spacer, HorizontalBox, VerticalBox) are recycled from a LIFO pool instead of being allocated/deallocated every mount cycle.
- **Lazy slot creation** -- Children that mount with `visibility: collapsed` skip expensive slot configuration (style resolution, alignment parsing, padding conversion, flex sizing). Config is applied automatically when the child transitions to visible.
- **React Suspense support** -- Added `hideInstance`/`unhideInstance`/`hideTextInstance`/`unhideTextInstance` hooks so `<Suspense>` boundaries correctly toggle content visibility.

### Test Results
**103 tests passing** across converters, parsers, containers, and utilities. Zero regressions from any optimization work.

---

## System Requirements

| Requirement | Version |
|-------------|---------|
| Unreal Engine | **5.x** |
| Node.js | **>= 18** |
| Package Manager | Yarn, npm, or pnpm |
| Editor | VSCode / Cursor recommended |
| OS | Windows 10/11, macOS, Linux |

---

## Installation

### 1. Clone the plugin into your project

```
cd YourProject/Plugins
git clone https://github.com/Kemerd/ReactorUMG.git
```

Or add as a submodule:

```
git submodule add https://github.com/Kemerd/ReactorUMG.git Plugins/ReactorUMG
```

### 2. Run the setup script

The setup script downloads V8 engine binaries, scaffolds the TypeScript workspace, and installs dependencies:

**Windows (bat):**
```
Plugins\ReactorUMG\Tools\setup_win.bat
```

**Windows (Python -- handles symlinks correctly):**
```
python Plugins\ReactorUMG\Tools\setup_win.py
```

The script will:
1. Download and extract V8 engine binaries (if not already present)
2. Copy the TypeScript project template into `<ProjectRoot>/TypeScript/`
3. Run `yarn build` to compile everything

### 3. Launch the project

Open your `.uproject` in Unreal Editor. The plugin loads automatically.

### 4. Generate UE type declarations (optional but recommended)

In the Editor console, run:

```
ReactorUMG.GenDTS
```

This generates TypeScript type definitions for all your UE classes under `TypeScript/types/ue/`, giving you full autocomplete for engine APIs.

---

## Usage

### Creating an Editor UI Tool

1. In the Content Browser, right-click and choose **ReactorUMG > EditorUtilityWidget**
2. Name your asset (e.g., `MyToolPanel`)
3. Write your UI code in `TypeScript/<ProjectName>/Editor/<AssetName>/`

### Writing React Components

Your TypeScript files live under `<ProjectRoot>/TypeScript/`. Write standard React components using TSX:

```tsx
import * as React from 'react';
import { useState } from 'react';

const HealthBar = ({ current, max }: { current: number; max: number }) => {
  const pct = Math.round((current / max) * 100);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
      <text style={{ color: '#ffffff', fontSize: '14px', fontFamily: 'Roboto' }}>
        HP: {current} / {max}
      </text>
      <progress
        value={current}
        max={max}
        style={{ width: '200px', height: '20px' }}
      />
    </div>
  );
};

export default HealthBar;
```

### Available HTML-like Elements

These JSX tags map directly to UMG widgets:

| JSX Tag | UMG Widget | Notes |
|---------|------------|-------|
| `<div>` | HorizontalBox / VerticalBox / CanvasPanel / Overlay | Determined by `display` and `position` CSS |
| `<text>`, `<span>`, `<p>`, `<h1>`-`<h6>` | TextBlock / WrapBox | Rich text with font styling |
| `<button>` | Button | Click, hover, focus events |
| `<img>` | Image | `src` path, tint, objectFit |
| `<input>` | EditableText / CheckBox / Slider | `type` determines widget |
| `<textarea>` | MultiLineEditableText | onChange, onSubmit, onBlur |
| `<select>` / `<option>` | ComboBox | Dropdown selection |
| `<progress>` | ProgressBar | Determinate and marquee modes |
| `<video>` | MediaPlayer widget | Basic media playback |
| `<audio>` | Audio component | Sound playback |
| `<a>` | Clickable text | URL opening |
| `<label>` | TextBlock | Semantic text container |
| `<style>` | Inline CSS registry | Class/ID selectors |

### Using UMG Widgets Directly

You can also use native UMG widgets as React elements:

```tsx
<Border backgroundColor="#1a1a2e" contentPadding="16px">
  <SizeBox widthOverride={400} heightOverride={300}>
    <ScrollBox orientation="vertical">
      <TextBlock text="Direct UMG widget access" />
    </ScrollBox>
  </SizeBox>
</Border>
```

### Supported CSS Properties

| Category | Properties |
|----------|-----------|
| **Layout** | `display` (flex/grid), `flexDirection`, `flexWrap`, `justifyContent`, `alignItems`, `alignSelf`, `gap`, `position` (static/relative/absolute/fixed) |
| **Sizing** | `width`, `height`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`, `aspectRatio` |
| **Spacing** | `margin`, `padding` (all shorthand variants) |
| **Visual** | `backgroundColor`, `backgroundImage`, `opacity`, `visibility`, `overflow` (hidden/scroll/auto) |
| **Typography** | `fontSize`, `fontFamily`, `fontWeight`, `color`, `textAlign`, `textTransform`, `lineHeight`, `letterSpacing` |
| **Border** | `border`, `borderRadius`, `boxShadow` |
| **Transform** | `transform` (translate, rotate, scale), `transformOrigin` |
| **Animation** | `transition` (property interpolation), CSS `@keyframes` |
| **Other** | `cursor`, `zIndex`, `objectFit`, `scale` |

### Event Handling

Standard React event handlers work with bubbling and capturing:

```tsx
<button
  onClick={() => console.log('clicked')}
  onMouseEnter={() => setHovered(true)}
  onMouseLeave={() => setHovered(false)}
  onKeyDown={(e) => console.log('key:', e.key)}
>
  Hover me
</button>
```

### Building for Production

From your `TypeScript/` directory:

```bash
yarn build          # Full compile
yarn build:watch    # Watch mode for development
yarn dev            # Webpack dev build
yarn pack           # Webpack production build
```

Make sure `Additional Non-Asset Directories to Package` in your project settings includes the `JavaScript` directory, or your UI won't load in packaged builds.

---

## Project Structure

```
YourProject/
  Plugins/
    ReactorUMG/
      Source/
        ReactorUMG/         # Core C++ plugin (widget management, PuerTS bridge)
        ReactorUMGEditor/   # Editor module (asset factories, tooling)
        Puerts/             # PuerTS JavaScript engine integration
        JsEnv/              # JS environment runtime
      Scripts/
        Project/            # TypeScript template (copied to project on setup)
      Tools/
        setup_win.bat       # Windows setup script
        setup_win.py        # Python setup (symlink-safe)
      ReactorUMG-TS/        # React reconciler TypeScript package
        renderer.ts         # React reconciler host config
        converter.ts        # Element factory (routes types to converters)
        container/          # Layout converters (flex, grid, canvas, overlay)
        jsx/                # HTML tag converters (button, text, img, input, etc.)
        umg/                # Direct UMG widget converters (25+ predefined)
        parsers/            # CSS property parsers (color, font, margin, etc.)
        events.ts           # Event bubbling/capturing dispatcher
        perf/               # Performance modules (batch_sync, widget_pool)
        test/               # 103 mocha tests
  TypeScript/               # Your project's UI code (created by setup script)
    src/
      <ProjectName>/
        Editor/             # Editor tool UIs
        Runtime/            # Game runtime UIs
    package.json
    tsconfig.json
```

---

## FAQ

**Q: What's the relationship with native UMG/Slate?**
ReactorUMG translates your React component tree into real UMG widgets at runtime. It's not a replacement for UMG -- it's a developer experience layer on top of it. Every `<div>`, `<button>`, and `<text>` becomes a native UMG widget under the hood.

**Q: Can I mix ReactorUMG with Blueprint UMG?**
Yes. ReactorUMG renders into a standard `UWidgetTree`. You can embed ReactorUMG panels inside Blueprint widgets and vice versa.

**Q: How do I update the PuerTS type definitions?**
Run `ReactorUMG.GenDTS` in the Editor console. This regenerates `TypeScript/types/ue/` with fresh declarations for all your project's classes.

**Q: My UI doesn't show up in a packaged build.**
Make sure `Additional Non-Asset Directories to Package` includes the `JavaScript` directory in your project settings.

**Q: How do I call C++ / Blueprint APIs from TypeScript?**
PuerTS gives you full access to the engine API. After running `GenDTS`, you get typed access to any `UFUNCTION`, `UPROPERTY`, or Blueprint-exposed API:

```tsx
import * as UE from 'ue';
const playerController = UE.GameplayStatics.GetPlayerController(world, 0);
```

---

## Runtime Platforms

| Platform | Status |
|----------|--------|
| Windows | Supported |
| Linux | Supported |
| macOS | Supported |
| Android | Supported |
| iOS | Supported |

---

## Credits

- **Original project**: [Caleb196x/ReactorUMG](https://github.com/Caleb196x/ReactorUMG) -- the architecture, C++ plugin, and initial TypeScript package
- **PuerTS**: [puer-tech/puerts](https://github.com/puer-tech/puerts) -- the TypeScript/JavaScript engine bridge for Unreal
- **This fork**: Production upgrade across all six phases -- reconciler fixes, component implementations, CSS/layout completion, event system, C++ improvements, and performance optimizations

---

## License

**MIT License**. See [LICENSE](./LICENSE) for details.
