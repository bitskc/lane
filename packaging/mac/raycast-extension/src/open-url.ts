import { showHUD, LaunchProps } from "@raycast/api";
import { execFile } from "child_process";
import { promisify } from "util";

const execFileAsync = promisify(execFile);

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
  try {
    await execFileAsync(lane, [url]);
  } catch (error) {
    await showHUD("❌ Could not reach Lane.app");
  }
}
