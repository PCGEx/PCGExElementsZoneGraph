![UE 5.7](https://img.shields.io/badge/UE-5.7-darkgreen)

<p align="center">
  <img src="https://github.com/Nebukam/PCGExElementsZoneGraph/blob/main/Resources/ZG_Logo.png" alt="PCGEx Logo">
</p>

<h1 align="center">PCGEx + ZoneGraph</h1>

<p align="center">
  <strong>Generate ZoneGraph data for AI navigation from PCGEx clusters</strong><br>
  Convert PCGEx cluster topology into ZoneGraph lanes and zones.
</p>

<p align="center">
  <a href="https://pcgex.gitbook.io/pcgex/zone-graph">Documentation</a> •
  <a href="https://discord.gg/mde2vC5gbE">Discord</a> •
  <a href="https://www.fab.com/listings/e9d0c0a1-5bae-4dc8-8d17-3f1a1877bf91">FAB</a>
</p>

---

## What is PCGEx + ZoneGraph?

This is a **companion plugin for [PCGEx](https://github.com/Nebukam/PCGExtendedToolkit)** that bridges PCGEx cluster data with Epic's [ZoneGraph plugin](https://dev.epicgames.com/community/learning/tutorials/qz6r/unreal-engine-zonegraph-quick-start-guide), converting cluster topology into ZoneGraph data for AI navigation.

### Experimental Status

Epic's ZoneGraph plugin is **experimental**. Epic warns that "ZoneGraph will have API breaking changes as its development progresses." If something breaks, [open an issue](https://github.com/Nebukam/PCGExElementsZoneGraph/issues) and I'll look into it.

---

## Requirements

- **Unreal Engine 5.7+**
- **[PCGExtendedToolkit](https://github.com/Nebukam/PCGExtendedToolkit)** — Core PCGEx plugin (free, MIT licensed)
- **ZoneGraph** — Epic's experimental ZoneGraph plugin (included with the engine)

---

## Installation

### From FAB

Get the latest release from the **[FAB Marketplace](https://www.fab.com/listings/e9d0c0a1-5bae-4dc8-8d17-3f1a1877bf91)**.

### From Source

1. Clone this repository into your project's `Plugins/` folder
2. Ensure **PCGExtendedToolkit** and **ZoneGraph** are both enabled
3. Regenerate project files and build

---

## License

> **This plugin is source-available**

- **Personal use** — Free. Individuals may use, copy, and modify the plugin for non-commercial and personal projects at no cost.
- **Studio / Professional use** — Requires a paid license, available through [FAB](https://www.fab.com/listings/e9d0c0a1-5bae-4dc8-8d17-3f1a1877bf91) under the Professional pricing tier.
- **Custom licensing** — Alternative licensing arrangements are available and are not bound to the FAB platform. Contact me directly to discuss terms.

See [LICENSE](LICENSE) for full terms.

---

## Support

- **[Discord](https://discord.gg/mde2vC5gbE)** — Community support and discussion
- **[Documentation](https://pcgex.gitbook.io/pcgex)** — Guides and tutorials
- **[Patreon](https://www.patreon.com/c/pcgex)** — Support PCGEx development
