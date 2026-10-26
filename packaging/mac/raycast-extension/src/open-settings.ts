import { showHUD } from "@raycast/api";
import { execFile } from "child_process";
import { promisify } from "util";

const execFileAsync = promisify(execFile);

export default async function Command() {
  const lane = "/Applications/Lane.app/Contents/MacOS/lane";
  try {
    await execFileAsync(lane, ["--settings"]);
  } catch (error) {
    await showHUD("❌ Could not reach Lane.app");
  }
}
