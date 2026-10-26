#!/bin/bash

# Required parameters:
# @raycast.schemaVersion 1
# @raycast.title Open Clipboard URL in Lane
# @raycast.mode silent

# Optional parameters:
# @raycast.icon 🔗
# @raycast.packageName Lane

# Routes the clipboard's URL through Lane's rules/remembered/picker
# pipeline. Add "-p" to force the picker instead of remembered targets.
LANE="/Applications/Lane.app/Contents/MacOS/lane"
if [ ! -x "$LANE" ]; then
  echo "Lane.app is not installed in /Applications" >&2
  exit 1
fi
exec "$LANE" --clipboard
