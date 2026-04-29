#ifndef FIRMWARE_VERSION_H
#define FIRMWARE_VERSION_H

// FIRMWARE_VERSION is injected by version.py at build time from `git describe`.
// Falls back to "dev" outside a git checkout.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

#define GITHUB_OWNER "bluescripts-net"
#define GITHUB_REPO "garden.gg-iot"
#define FIRMWARE_ASSET_NAME "firmware.bin"

#endif
