import { showHUD } from "@raycast/api";
import { execFile } from "child_process";

export default async function Command() {
  const lane = "/Applications/Lane.app/Contents/MacOS/lane";
  execFile(lane, ["--settings"], (error) => {
    if (error) {
      showHUD("❌ Could not reach Lane.app");
    }
  });
}
