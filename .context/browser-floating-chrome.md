# Solid browser shell and rounded page panel

- The Causality title bar and left tab panel share the theme's solid canvas
  color. The title bar provides window drag, minimize, maximize, and close.
  The tab panel keeps its compact and expanded group layouts.
- The page has a one-point theme keyline and 14-point inner corners. The
  right and bottom have four-point gutters painted with the same color
  as the title and left panel. The Start Page uses the same frame.
- `src/chrome/page_overlay_mac.m` clips Causality's existing native WebViews
  to the rounded page surface. It uses a separate dock cutout so the
  Causality address field and buttons remain clickable on macOS 26+.
  Older builds put the dock in the bottom lane.
- Native macOS verification covered rendered corners, title and rail color,
  dock hit routing, live navigation, tabs, menu, resizing, and maximize.
  Windows and Linux runtime behavior remains unverified.
