# Primary references and provenance

Vendor files are not redistributed. Match document revisions and delivered component
markings before applying electrical or protocol details.

- [Hi-Link HLK-LD2451 product/download page](https://www.hlktech.com/en/Goods-245.html): manufacturer entry point for radar manuals and serial protocol.
- [HLK-LD2451 user manual V1.03](https://h.hlktech.com/download/HLK-LD2451-24G/1/HLK-LD2451%E4%BD%BF%E7%94%A8%E6%89%8B%E5%86%8C_V1.03%20.pdf): physical/electrical reference, dated 2024-08-21. Historical captures used the earlier V1.00 manual too.
- HLK-LD2451 Serial Communication Protocol V1.03, dated 2024-07-01: source for the project's [protocol notes](protocol.md). Find the serial-protocol download through the manufacturer page; it is distinct from the user manual. Preserve the documented direction and ACK discrepancies.
- [TI SN74LVC4245A datasheet](https://www.ti.com/lit/ds/symlink/sn74lvc4245a.pdf): voltage domains and function table; does not establish breakout routing.
- [Espressif ESP32-S3-WROOM-1/1U datasheet](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf): module/PSRAM constraints; not a clone-carrier schematic.
- [PlatformIO ESP32-S3 target](https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html): build profile; this project overrides flash/PSRAM settings.
- [DeWalt DCB205](https://www.dewalt.com/en-us/product/dcb205/20v-max-5ah-battery): pack identification, not project runtime or cutoff validation.
- [Littelfuse ATOF reference](https://www.littelfuse.cn/assetdocs/littelfuse-datasheet-287-atof?assetguid=43dcdce8-8ca2-426f-8998-7e566f048d40): fuse reference; installed part ratings still need matching.
- [Freetronics DMD source](https://github.com/freetronics/DMD): consulted for panel scan order/polarity; not included or linked. See [notices](../THIRD_PARTY_NOTICES.md).

No manufacturer schematic was established for the exact P10(1R)-4S-V10 panel or
clone carrier. The connection tables describe the recorded specimen and cable,
not an inferred generic connector standard. Seller ratings remain identified as such.
