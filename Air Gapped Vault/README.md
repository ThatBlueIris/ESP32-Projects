# Air-Gapped ESP32-S3 Hardware Vault

A standalone, offline password and credential manager built on an ESP32-S3. Operates without internet access to keep sensitive tokens physically secure.

## Key Features

* **Air-Gapped Access**: Isolated offline Wi-Fi AP (`ESP32-VAULT-AIRGAP`) with captive portal redirection. Zero internet uplink.
* **Physical 2FA**: Entering a valid PIN starts a 15-second window requiring a physical tap on the board's **BOOT** button to finalize login.
* **Plausible Deniability (Dual Vaults)**:
  * **Real PIN (`1337`)**: Loads the `sec_real` NVS flash namespace containing real credentials.
  * **Decoy PIN (`9999`)**: Loads an isolated `sec_decoy` namespace with dummy tokens under coercion.
* **Instant Hardware Panic Lock**: Pressing the physical **BOOT** button while unlocked immediately purges the web session and forces a browser lockout.
* **Auto-Lock Timeout**: Automatically locks after 90 seconds of web inactivity.
* **Full Secret CRUD**: Create, read (with mask/unmask), copy to clipboard (with mobile HTTP fallback), and delete secrets.
* **Hardware TRNG Password Generator**: Uses the ESP32-S3 silicon True Random Number Generator (`esp_random()`) to yield 20-character high-entropy passwords.
* **Brute-Force Rate Limiting**: Escalating cooldown penalties kick in after 3 consecutive failed PIN attempts.
* **Physical Factory Reset**: Holding the **BOOT** button for 5 seconds while at the locked screen sanitizes all NVS partitions.

## Hardware Specs & Controls

* **Hardware**: ESP32-S3
* **RGB LED (GPIO 48)**: Dim Amber (Locked) | Dim Blue (Presence Wait) | Dim Green (Unlocked) | Red Strobe (Denied / Wipe)
* **Control Switch**: Physical BOOT Button (GPIO 0)
* **Access Point SSID**: `ESP32-VAULT-AIRGAP` (Pass: `vault1234`)
* **Default Master PINs**: Real: `1337` | Decoy: `9999`
