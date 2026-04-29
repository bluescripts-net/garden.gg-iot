"""PlatformIO pre-build script that injects FIRMWARE_VERSION from `git describe`.

Tag the repo with CalVer (e.g., v2026.04.28.0) and the build will bake
that version into the firmware. Falls back to commit SHA, or "dev"
outside a git checkout.
"""
import subprocess

Import("env")  # noqa: F821 — provided by PlatformIO


def get_version():
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            capture_output=True, text=True, check=True,
        )
        version = result.stdout.strip()
        if version.startswith("v"):
            version = version[1:]
        return version or "dev"
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


version = get_version()
print(f"FIRMWARE_VERSION = {version}")
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
