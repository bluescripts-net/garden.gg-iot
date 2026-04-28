#!/usr/bin/env python3
"""
Auto-provision a fresh GardenGG device from the command line (Windows).

Reads credentials from environment variables (or a local .env file you
create yourself — never commit it):

    GARDENGG_WIFI_SSID    target WiFi network the device should join
    GARDENGG_WIFI_PASS    its password
    GARDENGG_API_KEY      garden.gg API key (gg_live_...)
    GARDENGG_PLOT_ID      garden.gg plot UUID
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
import time

import requests

SSID = os.environ.get("GARDENGG_WIFI_SSID")
PASS = os.environ.get("GARDENGG_WIFI_PASS")
API_KEY = os.environ.get("GARDENGG_API_KEY")
PLOT_ID = os.environ.get("GARDENGG_PLOT_ID")
INTERVAL_MS = int(os.environ.get("GARDENGG_INTERVAL_MS", "900000"))

GARDENGG_AP_PREFIX = "GardenGG-Setup-"
PORTAL_IP = "192.168.4.1"
PORTAL_URL = f"http://{PORTAL_IP}/save"


def require_env():
    missing = [k for k, v in {
        "GARDENGG_WIFI_SSID": SSID,
        "GARDENGG_WIFI_PASS": PASS,
        "GARDENGG_API_KEY": API_KEY,
        "GARDENGG_PLOT_ID": PLOT_ID,
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


def connect_to_ap():
    result = subprocess.run(
        ["netsh", "wlan", "show", "networks"],
        capture_output=True, text=True,
    )
    ssid = None
    for line in result.stdout.split("\n"):
        if GARDENGG_AP_PREFIX in line and "SSID" in line:
            parts = line.split(":")
            if len(parts) > 1:
                ssid = parts[1].strip()
                break
    if not ssid:
        print("Could not find GardenGG AP SSID")
        return False

    print(f"Connecting to {ssid}...")
    subprocess.run(["netsh", "wlan", "connect", f"name={ssid}"], capture_output=True, text=True)

    for i in range(20):
        time.sleep(1)
        check = subprocess.run(
            ["netsh", "wlan", "show", "interfaces"],
            capture_output=True, text=True,
        )
        if "connected" in check.stdout.lower():
            print(f"Connected to {ssid}")
            return True
        print(f"  Waiting... ({i+1}/20)")
    return False


def submit_form():
    print(f"Submitting provisioning form to {PORTAL_URL}...")
    data = {
        "ssid": SSID,
        "pass": PASS,
        "api_key": API_KEY,
        "plot_id": PLOT_ID,
        "interval": INTERVAL_MS,
    }
    try:
        response = requests.post(PORTAL_URL, data=data, timeout=10)
        print(f"Response: {response.status_code}")
        return response.status_code == 200
    except Exception as e:
        print(f"Error submitting form: {e}")
        return False


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


def main():
    print("=== GardenGG Provisioning Automation ===\n")
    require_env()

    if not find_gardengg_network():
        print("ERROR: Could not find GardenGG WiFi network")
        print("Make sure the device is powered on and broadcasting the AP")
        sys.exit(1)

    if not connect_to_ap():
        print("ERROR: Could not connect to GardenGG AP")
        sys.exit(1)

    time.sleep(2)

    if not submit_form():
        print("ERROR: Could not submit provisioning form")
        sys.exit(1)

    if not wait_for_reconnect():
        print("WARNING: Could not confirm device reconnected to main WiFi")

    print("\n[ok] Provisioning complete!")
    print(f"Device should now be capturing photos every {INTERVAL_MS // 1000}s")


if __name__ == "__main__":
    main()
