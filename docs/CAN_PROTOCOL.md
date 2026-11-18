# Superseded CAN Protocol Note

The primary receiver-facing protocol for this project is now CANaerospace, documented in `docs/CANAEROSPACE_PROTOCOL.md`.

The earlier `0x301` / `0x302` proprietary payload layout was useful during bring-up, but it is not CANaerospace-compliant because bytes 0..3 are not the CANaerospace Node-ID, Data Type, Service Code, and Message Code header. Do not use that old layout as the production contract.
