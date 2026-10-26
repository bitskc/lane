import { showHUD } from "@raycast/api";
import { execFile } from "child_process";

export default async function Command() {
  const lane = "/Applications/Lane.app/Contents/MacOS/lane";
  execFile(lane, ["--rediscover"], async (error) => {
    if (error) {
      await showHUD("❌ Could not reach Lane.app");
    } else {
      await showHUD("🔄 Lane targets rescanned");
    }
  });
}
