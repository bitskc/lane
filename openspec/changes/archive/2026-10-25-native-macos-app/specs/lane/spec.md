## MODIFIED Requirements

### Requirement: System tray presence
Lane SHALL show a tray icon on platforms with a status-icon mechanism; activating it SHALL open the Settings window, and its context menu SHALL offer Settings and Rediscover browsers. On Linux the tray SHALL NOT offer Quit (the resident daemon owns link routing). On macOS the status-item menu SHALL additionally offer Quit Lane, because LaunchServices relaunches Lane on the next link click while Lane is the registered handler and a status item without Quit is not discoverable there.

#### Scenario: Tray activation
- **WHEN** the user clicks Lane's tray icon on Linux
- **THEN** the Settings window opens or raises

#### Scenario: Status-item activation on macOS
- **WHEN** the user clicks Lane's status item on macOS
- **THEN** the status-item menu opens, and choosing Settings opens or raises the Settings window

#### Scenario: Tray menu
- **WHEN** the user opens the tray context menu on Linux
- **THEN** it offers "Settings" and "Rediscover browsers" and no Quit entry

#### Scenario: Status-item menu on macOS
- **WHEN** the user opens the status-item menu on macOS
- **THEN** it offers "Settings", "Rediscover browsers" and "Quit Lane"

## ADDED Requirements

### Requirement: macOS key hints match the real shortcuts
On macOS the picker footer, empty-state hints and the accessible names and descriptions of the picker controls SHALL show the macOS key names for the shortcuts that actually fire, taken from one per-platform table that also drives the shortcuts, so hint and shortcut cannot drift. Linux strings SHALL be unchanged. Escape SHALL still clear a non-empty field before cancelling, and digit keys, Return and arrows SHALL behave as on Linux.

#### Scenario: Copy shortcut
- **WHEN** the picker has focus on macOS and the user presses Command+C
- **THEN** the current URL is copied and the footer and accessible description name Command+C, not Control+C

#### Scenario: Letter shortcuts with a focused text field
- **WHEN** the filter field has focus and the user presses each macOS letter shortcut shown in the footer
- **THEN** its action fires and no stray character appears in the field

#### Scenario: Accessible names follow the table
- **WHEN** an accessibility client reads the email or private-window control on macOS
- **THEN** the shortcut it announces is the same one shown in the footer

#### Scenario: Escape ladder
- **WHEN** the filter field is non-empty and the user presses Escape twice
- **THEN** the first press clears the field and the second cancels the picker

#### Scenario: Linux hints unchanged
- **WHEN** the picker runs on Linux
- **THEN** it shows the existing `alt+...` and `^C` strings

### Requirement: macOS Dock presence, quit and Settings window
On macOS Lane SHALL run as an accessory app with no Dock icon. The status-item Quit Lane entry and the standard Quit shortcut in the Settings, picker and Hold windows SHALL share one quit path: if a pick is pending in the picker or Hold, the request SHALL cancel that pick and Lane SHALL keep running; otherwise Lane SHALL quit. When no `config.json` exists at launch, Lane SHALL open the Settings Overview, except that a first launch caused by a link SHALL route that link first and open Settings only after the pick or launch completes. Closing the Settings window SHALL hide it and leave routing running. The status-item icon SHALL be a template image that follows light and dark appearance.

#### Scenario: Quit with nothing pending
- **WHEN** the user chooses Quit Lane from the status item and no pick is pending
- **THEN** Lane quits

#### Scenario: Quit with a pending pick
- **WHEN** a URL is pending in the picker or Hold and a quit is requested by shortcut or menu
- **THEN** that pick is cancelled and Lane keeps running

#### Scenario: Relaunch after quit
- **WHEN** Lane is the registered handler, has quit, and the user clicks a link
- **THEN** the system starts Lane and the link is routed once

#### Scenario: First launch
- **WHEN** Lane starts on macOS with no `config.json` and no link to route
- **THEN** the Settings Overview opens

#### Scenario: First launch caused by a link
- **WHEN** Lane starts on macOS with no `config.json` because the user clicked a link
- **THEN** the link is routed first and the Settings Overview opens only after that pick or launch completes

#### Scenario: Close Settings
- **WHEN** the user closes the Settings window
- **THEN** the window hides, Lane keeps running, and choosing Settings from the status item shows it again

#### Scenario: No Dock icon
- **WHEN** Lane is running
- **THEN** no Dock icon is shown

### Requirement: macOS default-handler states in Settings
The Overview page on macOS SHALL show the current http and https handler by application name, SHALL show distinct states for Registering, Registered, Declined, Partial and another copy of Lane, and SHALL never display Registered until both schemes re-query to the running Lane. When notifications are denied, Overview SHALL show the notifications-off banner with a button that opens system notification settings.

#### Scenario: State shown while the system prompt is open
- **WHEN** the user pressed the default-browser button and the system prompt has not been answered
- **THEN** Overview shows Registering and the button is disabled

#### Scenario: Current handler named
- **WHEN** another application is the handler
- **THEN** Overview names that application before any button press

### Requirement: macOS appearance and displays
On macOS the picker, Hold and Settings SHALL follow the system light or dark appearance live, SHALL disable Hold ring and dim animations when the system reduced-motion preference is on, and SHALL open the picker on the display containing the pointer even when the frontmost app is on another display.

#### Scenario: Appearance changes while open
- **WHEN** the system switches from light to dark while the picker is open
- **THEN** the picker recolors without restarting

#### Scenario: Reduced motion
- **WHEN** reduce motion is enabled in system settings
- **THEN** the Hold ring and the dim fade do not animate

#### Scenario: Pointer on the second display
- **WHERE** a second display is attached and the pointer is on it when a link opens the picker
- **THEN** the picker appears on that display
