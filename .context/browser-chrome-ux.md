# Browser chrome and Start Page

- `src/browser/window.c` builds the shared bottom dock, native Start Page, and
  browser command menu beside the tab rail. The custom title bar still
  reports the active WebView title. The address field shows Go, Reload, or
  Stop based on editing and page state.
- `src/browser/address.c` keeps CLI and native navigation validation separate
  from typed address-or-search routing. Search queries are UTF-8 byte encoded
  for DuckDuckGo when submitted.
- The menu owns Start Page, copy address, page zoom, and preset theme commands.
  It fills the workspace when the area to the right of the rail is narrower
  than 566 logical pixels. Both layouts keep the native WebViews alive.
- The Start Page swaps visibility with the WebView wrapper around about:blank
  and participates in the WebView's back and forward history.
- `src/appearance/theme.c` uses proportional OS fonts when available through
  the public Causality instance descriptor. Causality supplies icon glyphs
  and the fallback typography. No font assets are required beside the binary.
- Universal macOS Release build passed. A macOS integration check clicked
  through the Start Page, navigation, menu, zoom, clipboard, and theme at
  1000- and 480-pixel widths. Address routing and strict C syntax checks
  passed. Windows and Linux UI behavior has not been runtime-tested.
