import { showHUD, LaunchProps } from "@raycast/api";
import { execFile } from "child_process";

interface Arguments {
  url: string;
}

export default async function Command(props: LaunchProps<{ arguments: Arguments }>) {
  let url = props.arguments.url ? props.arguments.url.trim() : "";
  if (!url) return;
  if (!url.includes("://") && !url.includes(":")) {
    url = `https://${url}`;
  }
  const lane = "/Applications/Lane.app/Contents/MacOS/lane";
  execFile(lane, [url], (error) => {
    if (error) {
      showHUD("❌ Could not reach Lane.app");
    }
  });
}
