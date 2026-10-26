#!/bin/bash

# Required parameters:
# @raycast.schemaVersion 1
# @raycast.title Open URL in Lane
# @raycast.mode silent
# @raycast.argument1 { "type": "text", "placeholder": "URL (or bare host like developer.apple.com/enroll)" }

# Optional parameters:
# @raycast.icon 🌐
# @raycast.packageName Lane

LANE="/Applications/Lane.app/Contents/MacOS/lane"
if [ ! -x "$LANE" ]; then
  echo "Lane.app is not installed in /Applications" >&2
  exit 1
fi

url="$1"
# Bare host/path without a scheme: prepend https:// so the picker sees a
# routable URL (mirrors what clipboardUrlCandidate does for --clipboard).
case "$url" in
  *://*|*:*);;                      # has a scheme or scheme-like prefix
  "") exit 1;;
  *) url="https://$url";;
esac
exec "$LANE" "$url"
