# BricsCAD BRX on Linux — Field Notes

> Hard-won, verified findings from building `open-electrical` (a C++/BRX plugin)
> for **BricsCAD V26 on Linux (Fedora/Nobara)**. Most of this is undocumented or
> scattered across the SDK's `.txt` files and the `brxWxSample`. Everything here
> was confirmed on a real install: **BricsCAD V26.2.04**, BRX SDK V26, GCC 15,
> wxWidgets 3.2.8, kernel 7.0 (Nobara/Fedora 43).

If you read only one section, read **§6 (wxWidgets)** and **§7 (debugging)** —
they cost the most time to figure out.

---

## 1. Two different "roots": SDK vs. runtime

BRX development on Linux needs **two** separate trees, and confusing them is the
first trap:

| | Path (this machine) | Contains | Used for |
|---|---|---|---|
| **BRX SDK** | `/opt/brx_sdk_v26` | headers (`inc/`), Windows import libs (`lib64/*.lib`), samples, docs | **compiling** |
| **BricsCAD install** | `/opt/bricsys/bricscad/v26` | the runtime `.so` libraries + `libdrx_entrypoint.a` | **linking / running** |

Key gotcha: the SDK's `lib64/` only ships **Windows** `.lib`/`.pdb` files
(`brx26.lib`, `drx_entrypoint.lib`, …). On Linux you do **not** link those. The
real Linux libraries live in the **BricsCAD program folder**:

```
/opt/bricsys/bricscad/v26/
├── libbrx26.so            # the BRX API (acedRegCmds, acutPrintf, AcDb*, …)
├── libTD_Alloc.so         # Teigha/ODA allocator
├── libTD_Root.so          # Teigha/ODA root
├── libdrx_entrypoint.a    # ARX entry-point shim (MUST be whole-archived, §4)
├── libwx_*-3.1.so         # BricsCAD's OWN wxWidgets 3.1 (see §6!)
├── cadtask                # the actual CAD engine process (see §7)
├── bricscad               # the GUI launcher
└── bricscad.sh            # env-setup launcher script
```

`FindBRX.cmake` in this repo encodes exactly this split (`BRX_SDK_ROOT` +
`BRICSCAD_ROOT`).

---

## 2. The output is a `.lrx`, not a `.brx`

- **Windows** module extension: `.brx`
- **Linux** module extension: **`.lrx`** ("Linux Runtime eXtension")
- **macOS**: `.mrx`

A `.lrx` is just a shared object (`.so`) with a different extension. Load it with
`APPLOAD` or `(arxload "…/foo.lrx")`. Our CMake sets the suffix per-platform.

---

## 3. Compiler, defines, and the platform header

Confirmed working compile setup on Linux (mirrors `samples/brx/brxSample`):

- **Standard**: C++17, `-fPIC -fexceptions`.
- **Defines**: `_ACRXAPP  BRX_APP  __BRXTARGET=26  _UNICODE  UNICODE  _AFXEXT
  _FILE_OFFSET_BITS=64  _LARGE_FILES`.
- **Include dirs**: `<SDK>/inc` **and** `<SDK>/inc/Platform/substitutes`
  (the second holds *empty "fake" Windows headers* — `atlcom.h`, `afxwin.h`, … —
  so Windows-oriented BRX headers compile on Linux).
- **Platform header**: at the very top of your precompiled/entry header, before
  any other BRX include:

  ```cpp
  #ifndef _WIN32   // Linux, macOS
    #include "brx_platform_linux.h"   // the "Windows Platform Emulation Layer"
  #endif
  #include "arxHeaders.h"
  ```

  This emulation layer provides `CString`, `CArray`, `TCHAR`, a working
  `DllMain`, "Extension DLL" semantics, etc. **>90%** of the Windows BRX API is
  available on Linux through it.

The authoritative references (offline, inside the SDK):
`/opt/brx_sdk_v26/LinuxBuild.txt`, `KnownDifferences.txt`, `PortingManual.txt`,
and `samples/brx/brxSample/CMakeLists.txt`.

---

## 4. The single most important linker flag

A BRX module must **re-export the ARX entry points** so BricsCAD can find them.
On Linux those live in a static archive that must be linked **whole**:

```
-Wl,--whole-archive  /opt/bricsys/bricscad/v26/libdrx_entrypoint.a  -Wl,--no-whole-archive
```

Symptom if you forget it: the `.lrx` builds fine, but BricsCAD silently refuses
it or exposes no commands. Verify the exports are present:

```bash
nm -D --defined-only open-electrical.lrx | grep -E 'acrxEntryPoint|acrxGetApiVersion'
# 000...  T acrxEntryPoint
# 000...  T acrxGetApiVersion
```

Those **two symbols are the entire public ABI** of a BRX module (the Windows
`.def` exports exactly the same two). Everything else can — and, per §6, *should*
— be hidden.

### "589 undefined symbols" is normal
`ldd` and `nm -D -u` will show hundreds of **undefined** `AcDb*`/`aced*` symbols
in your `.lrx`. That is expected: they resolve **at load time** from the already
running BricsCAD process (`libbrx26.so` et al.). You only need to *link* against
`libbrx26.so`, `libTD_Alloc.so`, `libTD_Root.so` so the linker is satisfied.

---

## 5. Commands: the hyphen and underscore rules

- **Global command names never take a leading underscore.** BricsCAD honours the
  underscore only on the *local* (localized) name. Registering global `_FOO`
  makes `FOO` unreachable.
- **Hyphens are allowed in the global name** (`EL-CONFIG`, `EL-ROOM`, …), but a
  hyphen is *not* a legal C++ identifier, so the `ACED_ARXCOMMAND_ENTRY_AUTO`
  macros can't build them. We register manually instead:

  ```cpp
  acedRegCmds->addCommand(group, L"EL-CONFIG", L"EL-CONFIG", ACRX_CMD_MODAL, &fn);
  // …and acedRegCmds->removeGroup(group) in On_kUnloadAppMsg.
  ```

- **`On_kInitAppMsg` runs at load, but not necessarily with a document.** Do
  **not** touch the drawing database there — reading XData/dictionaries during
  module init is fragile. Defer DB access to the first command or to a
  `AcApDocManagerReactor::documentActivated` callback (a document is guaranteed
  active there).

---

## 6. wxWidgets: the crash that cost the most time

**This is the crux of GUI plugins on BricsCAD Linux.** BricsCAD itself is a
wxWidgets application and **ships + loads its own wxWidgets 3.1**
(`libwx_*-3.1.so` in the program folder). Two independent mistakes each crash the
host on load/use:

### 6.1 Don't link the system's *shared* wxWidgets
If your module links the distro's shared `libwx_*-3.2.so`, the process ends up
with **two wxWidgets runtimes** (BricsCAD's 3.1 + yours 3.2). wxWidgets keeps
global singletons (`wxTheApp`, the `wxModule` chain, `wxClassInfo`/RTTI tables,
GTK init). Two of them → **crash during `dlopen`**.

**Fix (the Bricsys-supported approach, per `brxWxSample/WxWidgetsSetup.txt`):**
build wxWidgets **statically** and link it **privately**:

1. `configure --disable-shared --enable-unicode --with-gtk=3 --enable-vendor=oel
   CFLAGS=-fPIC CXXFLAGS=-fPIC` → static `.a` archives.
   (`scripts/build-wxstatic.sh` does this into `~/.local/opt/wxstatic-oel-*`.)
2. **Hide the wx symbols** so they cannot interpose with BricsCAD's wx. We use a
   linker **version script** (`src/exports.map`) that exports *only* the two ARX
   entry points and makes everything else local:

   ```
   { global: acrxEntryPoint; acrxGetApiVersion; local: *; };
   ```

Verify the isolation on the built module:

```bash
readelf -d open-electrical.lrx | grep NEEDED | grep -i wx   # → nothing (good)
nm -D --defined-only open-electrical.lrx | grep -c ' T '     # → 2 (only entry points)
```

The only `libwx-3.1` references that remain come **transitively from
`libbrx26.so`** (BricsCAD's own), which is correct. GTK stays a single shared
instance (both wx's use the system `libgtk-3`), which is fine.

### 6.2 Don't `wxEntryStart` — `wxInitialize` instead
Even with the static/isolated wx, a **second, subtler crash** appears the moment
you open a window: a fault deep in `libstdc++`'s RTTI
(`__si_class_type_info` vtable, "Invalid permissions for mapped object").

Cause: our bootstrap called `wxEntryStart(argc=0, nullptr)`. `wxEntryStart` is
for a program that is *taking over* the process (it wants a real `argv` and it
sets up the main loop). Using it to *embed* wx in a host that already owns the
event loop leaves wx/GTK half-initialised; creating the first top-level window
then dereferences invalid state → crash.

**Fix (from `brxWxSample/gui/MyWxApp.cpp`):** on Linux, embed wx with
`wxInitialize()`, register a minimal app factory, and never start your own loop:

```cpp
class OeWxApp : public wxApp { bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(OeWxApp);       // provides the app factory, no main()

bool ensureWxInitialized() {
    if (!wxInitialize()) return false;   // creates OeWxApp; inits GTK (idempotent)
    wxInitAllImageHandlers();
    return true;
}
// teardown: wxUninitialize();
```

BricsCAD runs the GTK/wx main loop; your modal dialogs pump their **own nested
loop** via `ShowModal()`, and modeless windows (palettes) live on BricsCAD's
loop. Both were verified working with this setup.

> Note: your wx instance has **no top-level window of its own** (BricsCAD's
> windows belong to BricsCAD's wx). `wxTheApp->GetTopWindow()` returns `nullptr`,
> so parent your dialogs/frames to `nullptr`.

### 6.3 Version matters, but isolation matters more
BricsCAD V26 ships wx **3.1**; the distro had **3.2.8**. Because we link wx
statically **and** hide its symbols, the version mismatch is irrelevant — the two
runtimes never see each other. (Matching 3.1 was impossible anyway: BricsCAD
ships only the 3.1 `.so`, no headers.)

---

## 7. Debugging is hard: BricsCAD runs inside a `bwrap` sandbox

The biggest surprise. On Linux, `bricscad` (the launcher) **fork/execs the CAD
engine `cadtask` inside a bubblewrap (`bwrap`) sandbox**. Consequences:

- **External `gdb` cannot follow the crash.** Attaching gdb to `bricscad` and
  even `set detach-on-fork off` only yields the launcher + two `bwrap` helper
  inferiors; the real `cadtask` runs in a separate namespace out of reach.
- **No system core dumps** are produced for the sandboxed crash
  (`coredumpctl` shows nothing for the plugin crash).
- BricsCAD uses **Crashpad** internally, but the minidump lands inside the
  sandbox and isn't readily accessible.

What **does** work, in order of usefulness:

1. **`crash_report.txt`** — BricsCAD writes a plaintext crash report with a
   short symbolized-ish stack to its **current working directory**. This is the
   fastest confirmation of *where* it died, e.g.:
   ```
   Illegal access: Invalid permissions for mapped object on address 0x...
   /opt/bricsys/bricscad/v26/libbcutils.so(+0xffaa7)
   /lib64/libstdc++.so.6(_ZTVN10__cxxabiv120__si_class_type_infoE+0x10)   ← RTTI
   ```
2. **Instrument your own module with file logging.** Since the sandbox can write
   to `$HOME`, append a flushed line to `~/oe-init.log` at every step. After a
   crash the **last line** pinpoints the faulting step. This repo ships this as
   `utilities/Diag.h` / `EL_DIAG_LOG`, opt-in via the env var `EL_DIAG=1`:
   ```
   kInit: DONE (module fully loaded)           ← load is fine
   wx: calling wxInitialize() → initialized OK
   ui:togglePalette before PaletteHost::toggle ← last line ⇒ crash was creating the window
   ```
   This single technique is what actually located the wx bug.
3. **Reproduce without hand-interaction.** Drop an auto-loader LISP where
   BricsCAD reads it and open a drawing so a document context exists:
   ```
   ~/Bricsys/BricsCAD/V26x64/en_US/Support/on_doc_load.lsp:
       (vl-catch-all-apply (function arxload) (list "…/open-electrical.lrx"))
   ```
   Then launch bounded by `timeout`. `scripts/debug-load.sh` automates this.

### Useful specifics discovered
- The CAD process is **`cadtask`**; the GUI launcher is **`bricscad`**; the env
  setup lives in **`bricscad.sh`** (`source bricscad.sh --env-only` exports
  `LD_LIBRARY_PATH`, `GDK_BACKEND=x11`, etc. without launching). It also has a
  built-in `--debug` (wraps gdb) and `--print-command` mode.
- **Environment variables DO cross the bwrap sandbox** — `EL_DIAG=1` set before
  launch reaches `cadtask`. Handy for opt-in diagnostics.
- **`(command "…")` does NOT run from inside `on_doc_load.lsp`** (it executes in
  a reactor context where the command function is blocked). Use it only to
  `arxload`; drive commands another way.
- `cadtask --help` / `bricscad --help` don't print help — they just launch.
- User config/support lives under `~/Bricsys/BricsCAD/V26x64/en_US/`
  (`Support/`, `Templates/`, `ERROR.LOG`, `appload.dfs` = the APPLOAD "Startup
  Suite").

---

## 8. Persistence that survives save/reload

Project data is stored in the drawing itself (not side files) so it travels with
the `.dwg`:

- A **named-object-dictionary** tree under the root key `EL_ELECTRICAL_PROJECT`,
  with sub-dictionaries (`Rooms`, `Elements`, `Circuits`, `Conduits`) each
  holding one **`AcDbXrecord`** per object.
- Each Xrecord is a **resbuf chain** built from DXF group codes
  (`1`=key, `40`=real, `90`=int, `300`=text, `10`=point). Register your app in
  the **RegApp** table before attaching **XData**.
- Gotcha carried over from AutoCAD ARX and confirmed on BRX: **transaction-
  resident objects are closed when the transaction ends** — don't touch them
  after commit. Open `ForRead`, `upgradeOpen()`/reopen `ForWrite` to modify.
- `resbuf`/`find_*` CMake gotcha (not BRX, but bit us): CMake's `find_file`/
  `find_library` **skip the search if the result variable is already set** — even
  to an empty string. Never pre-initialise those variables.

---

## 9. Symbols (blocks) can be generated in code

Rather than shipping a symbol `.dwg`, the plugin **creates its NBR 5444 block
definitions programmatically** on first use (`services/SymbolFactory`): build an
`AcDbBlockTableRecord`, append `AcDbLine`/`AcDbCircle`/`AcDbArc` with
`setColorIndex(0)` (BYBLOCK) so the inserted reference takes the target layer's
colour. If a block with the same name already exists (user's template), the
plugin uses theirs and skips generation.

---

## 10. Quick reference — commands that helped

```bash
# Environment for building/running against a real install
source /opt/bricsys/bricscad/v26/bricscad.sh --env-only

# Verify a built .lrx
file open-electrical.lrx
readelf -d open-electrical.lrx | grep NEEDED
nm -D --defined-only open-electrical.lrx | grep ' T '        # exports (expect 2)
readelf -d open-electrical.lrx | grep -i wx                  # expect none direct

# Reproduce a load/UI crash with a backtrace-ish trace (sandbox-proof)
EL_DIAG=1 ./scripts/debug-load.sh          # then read ~/oe-init.log + ./crash_report.txt
```

---

### TL;DR
1. Link runtime `.so`s from the **BricsCAD install**, not the SDK. Output `.lrx`.
2. **Whole-archive `libdrx_entrypoint.a`** or the module won't register.
3. Include `brx_platform_linux.h` + `inc/Platform/substitutes`; define
   `_ACRXAPP BRX_APP __BRXTARGET=26`.
4. wxWidgets: link it **static + private** (version script exporting only the two
   entry points) and initialise it with **`wxInitialize()`**, never
   `wxEntryStart`.
5. The engine runs in a **bwrap sandbox** — debug with `crash_report.txt` +
   in-module file logging, not gdb.
