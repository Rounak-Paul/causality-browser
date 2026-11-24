# Custom title bar and themes

- The Causality title bar and left tab panel share the theme canvas color.
  The active page title is set through `ca_window_set_title`.
- Appearance controls are in the bottom dock's browser menu. The menu lays
  out alongside the native WebView at desktop widths and replaces the
  visible workspace at narrow widths. The retained page stays alive.
- `src/appearance/theme.c` defines shared CSS and the Dark, Light, and Dusk
  palettes using Causality's `--ca-*` color roles. A change attaches a
  prepared author stylesheet and updates the window canvas without rebuilding
  the page. Causality restyles the title bar and existing widgets.
- Theme preferences live in CFPreferences on macOS, the current user's
  registry on Windows, and an XDG config file on Linux. Dark is the fallback.
