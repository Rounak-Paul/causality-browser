# Browser tab rail

- `src/browser/tabs.c` owns session tabs, groups, and the left rail. Compact
  tiles show site initials or a Causality icon. The bottom chevron switches
  to a solid 204-point list of titles and named groups. The compact slot
  is 46 points wide and keeps 36 points of usable width for its controls.
- At narrow windows the expanded rail fills the content area. Selecting a
  tab returns to browsing. Group folders can be opened and closed in either
  rail mode. Dragging onto a folder moves a tab. Dragging onto an ungrouped
  tab creates a folder for both.
- Tabs use independent Causality WebViews inside keyed wrappers. Selection
  updates native visibility, navigation controls, title, and per-tab zoom.
  Existing WebViews are reused when opening tabs. Closing the last tab
  creates a fresh Start Page. Group removal leaves member tabs open.
- Group and tab state is scoped to the running window. Causality's native
  WebView backend availability is unchanged.
- macOS integration verified compact and expanded geometry, live navigation,
  page retention, naming and assigning groups, folder toggles, drag-to-group,
  tab closing, and the full-width rail. The drag harness simulates macOS
  button state while exercising Causality's input dispatch. Windows and Linux
  runtime behavior remains unverified.
