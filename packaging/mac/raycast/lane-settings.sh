#!/bin/bash

# Required parameters:
# @raycast.schemaVersion 1
# @raycast.title Open Lane Settings
# @raycast.mode silent

# Optional parameters:
# @raycast.icon ⚙️
# @raycast.packageName Lane

LANE="/Applications/Lane.app/Contents/MacOS/lane"
if [ ! -x "$LANE" ]; then
  echo "Lane.app is not installed in /Applications" >&2
  exit 1
fi
exec "$LANE" --settings
