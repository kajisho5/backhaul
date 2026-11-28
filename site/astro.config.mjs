// The sales site: plain pages, English at "/", Japanese under "/ja/".
import { defineConfig } from "astro/config";

export default defineConfig({
  site: "https://backhaul.seventh-well.com",
  trailingSlash: "always",
  build: { format: "directory" },
});
