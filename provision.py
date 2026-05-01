#!/usr/bin/env python3
"""
Auto-provision a fresh GardenGG device from the command line (Windows).

Reads credentials from environment variables (or a local .env file you
create yourself — never commit it):

    GARDENGG_WIFI_SSID    target WiFi network the device should join
    GARDENGG_WIFI_PASS    its password
    GARDENGG_API_KEY      garden.gg API key (gg_live_...)
    GARDENGG_PLOT_ID      optional; pick later in the device's web UI
    GARDENGG_INTERVAL_MS  optional, defaults to 900000 (15 min)

Usage:
    set GARDENGG_WIFI_SSID=...
    set GARDENGG_WIFI_PASS=...
    set GARDENGG_API_KEY=...
    set GARDENGG_PLOT_ID=...
    python provision.py
"""
import os
import subprocess
import sys
import tempfile
import time
from xml.sax.saxutils import escape as xml_escape

import requests


def add_open_wifi_profile(ssid):
    """Create a one-shot Windows WiFi profile for an open SSID so that
    `netsh wlan connect` works. Idempotent: re-adding overwrites."""
    profile_xml = f"""<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
  <name>{xml_escape(ssid)}</name>
  <SSIDConfig>
    <SSID>
      <name>{xml_escape(ssid)}</name>
    </SSID>
  </SSIDConfig>
  <connectionType>ESS</connectionType>
  <connectionMode>manual</connectionMode>
  <MSM>
    <security>
      <authEncryption>
        <authentication>open</authentication>
        <encryption>none</encryption>
        <useOneX>false</useOneX>
      </authEncryption>
    </security>
  </MSM>
</WLANProfile>
"""
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".xml", delete=False, encoding="utf-8"
    ) as f:
        f.write(profile_xml)
        path = f.name
    try:
        subprocess.run(
            ["netsh", "wlan", "add", "profile", f"filename={path}", "user=current"],
            capture_output=True, text=True, check=True,
        )
    finally:
        os.unlink(path)


def remove_wifi_profile(ssid):
    """Best-effort cleanup of a one-shot provisioning profile."""
    subprocess.run(
        ["netsh", "wlan", "delete", "profile", f"name={ssid}"],
        capture_output=True, text=True,
    )

SSID = os.environ.get("GARDENGG_WIFI_SSID")
PASS = os.environ.get("GARDENGG_WIFI_PASS")
API_KEY = os.environ.get("GARDENGG_API_KEY")
PLOT_ID = os.environ.get("GARDENGG_PLOT_ID", "")
INTERVAL_MS = int(os.environ.get("GARDENGG_INTERVAL_MS", "900000"))

GARDENGG_AP_PREFIX = "GardenGG-Setup-"
PORTAL_IP = "192.168.4.1"
PORTAL_URL = f"http://{PORTAL_IP}/save"


def require_env():
    missing = [k for k, v in {
        "GARDENGG_WIFI_SSID": SSID,
        "GARDENGG_WIFI_PASS": PASS,
        "GARDENGG_API_KEY": API_KEY,
    }.items() if not v]
    if missing:
        print(f"ERROR: missing env vars: {', '.join(missing)}")
        sys.exit(2)


def find_gardengg_network():
    print(f"Scanning for {GARDENGG_AP_PREFIX}* network...")
    result = subprocess.run(
        ["netsh", "wlan", "show", "networks"],
        capture_output=True, text=True,
    )
    for line in result.stdout.split("\n"):
        if GARDENGG_AP_PREFIX in line:
            print(f"Found: {line.strip()}")
            return True
    return False


def find_gardengg_ssid():
    result = subprocess.run(
        ["netsh", "wlan", "show", "networks"],
        capture_output=True, text=True,
    )
    for line in result.stdout.split("\n"):
        if GARDENGG_AP_PREFIX in line and "SSID" in line:
            parts = line.split(":")
            if len(parts) > 1:
                return parts[1].strip()
    return None


def portal_reachable():
    """Return True if the captive-portal IP responds. Confirms DHCP done
    AND that Windows is actually still on the AP (not silently roamed)."""
    try:
        r = requests.get(f"http://{PORTAL_IP}/", timeout=2)
        return r.status_code in (200, 302, 404)
    except Exception:
        return False


def connect_to_ap():
    """Connect, then keep retrying if Windows roams off the no-internet AP."""
    ssid = find_gardengg_ssid()
    if not ssid:
        print("Could not find GardenGG AP SSID")
        return None

    print(f"Adding one-shot open-WiFi profile for {ssid}")
    add_open_wifi_profile(ssid)

    for attempt in range(3):
        print(f"Connecting to {ssid} (attempt {attempt + 1}/3)...")
        subprocess.run(
            ["netsh", "wlan", "connect", f"name={ssid}"],
            capture_output=True, text=True,
        )

        for i in range(20):
            time.sleep(1)
            if portal_reachable():
                print(f"Portal reachable at {PORTAL_IP}")
                return ssid
            print(f"  Waiting for portal... ({i+1}/20)")

        print("  Portal didn't respond, reconnecting...")

    return None


def submit_form():
    print(f"Submitting provisioning form to {PORTAL_URL}...")
    data = {
        "ssid": SSID,
        "pass": PASS,
        "api_key": API_KEY,
        "plot_id": PLOT_ID,
        "interval": INTERVAL_MS,
    }
    for attempt in range(3):
        if not portal_reachable():
            print(f"  Portal unreachable on attempt {attempt + 1}, reconnecting...")
            if not connect_to_ap():
                continue
        try:
            response = requests.post(PORTAL_URL, data=data, timeout=10)
            print(f"Response: {response.status_code}")
            if response.status_code == 200:
                return True
        except Exception as e:
            print(f"Error submitting form (attempt {attempt + 1}): {e}")
    return False


def main():
    print("=== GardenGG Provisioning Automation ===\n")
    require_env()

    if not find_gardengg_network():
        print("ERROR: Could not find GardenGG WiFi network")
        print("Make sure the device is powered on and broadcasting the AP")
        sys.exit(1)

    ap_ssid = connect_to_ap()
    if not ap_ssid:
        print("ERROR: Could not connect to GardenGG AP")
        sys.exit(1)

    time.sleep(2)
    submitted = submit_form()
    remove_wifi_profile(ap_ssid)

    if not submitted:
        print("ERROR: Could not submit provisioning form")
        sys.exit(1)

    if not wait_for_reconnect():
        print("WARNING: Could not confirm device reconnected to main WiFi")

    print("\n[ok] Provisioning complete!")
    print(f"Device should now be capturing photos every {INTERVAL_MS // 1000}s")


def wait_for_reconnect():
    print("Waiting for device to reboot and connect to WiFi...")
    time.sleep(5)
    print(f"Reconnecting to {SSID}...")
    subprocess.run(["netsh", "wlan", "connect", f"name={SSID}"], capture_output=True, text=True)
    for i in range(20):
        time.sleep(1)
        check = subprocess.run(
            ["netsh", "wlan", "show", "interfaces"],
            capture_output=True, text=True,
        )
        if "connected" in check.stdout.lower():
            print(f"Reconnected to {SSID}")
            return True
        print(f"  Waiting... ({i+1}/20)")
    return False


if __name__ == "__main__":
    main()
