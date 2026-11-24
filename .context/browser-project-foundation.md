# Browser project foundation

- The root CMake project builds the pinned `vendors/causality` and
  emits one browser executable in `bin/`. Build trees live in `build/`.
- `src/main.c` handles launch arguments. `src/browser/window.c` owns the
  single-window chrome and Causality WebView, and `address.c` resolves
  address-bar input to HTTP(S).
- Native backend availability comes from `ca_webview_supported()`. At
  `d24287d`, macOS hosts WKWebView and other desktop platforms have a stub.
- On macOS GLFW and Causality use the same static MoltenVK implementation.
  FreeType HVF is disabled to avoid a newer OS library dependency. On
  Windows the browser matches Causality's MSVC CRT mode.
- The macOS Release artifact is universal (arm64 and x86_64) and declares
  macOS 11 as its deployment target. Both slices followed an HTTP redirect
  and executed page JavaScript from a copied executable with the SDK
  environment cleared. The Intel slice ran under Rosetta. Windows, Linux,
  and the oldest macOS target have not been run-tested.
