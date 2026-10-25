# Inside a Drop of Saliva — website

RMIT D2D group installation.

- `index.html` — visitor page (opened by tapping the NFC tags). `#box1` … `#box4` jump to one box.
- `live.js` — live view of the LED boxes (used by both pages).
- `dev.html` — developer console (status, log, Wi-Fi, settings, remote program update). Needs the developer key set on the ESP32.
- `firmware/D2D_Saliva_Full.ino.bin` — put the exported program here for remote updates.

Hosted with GitHub Pages (Settings → Pages → Deploy from branch → main / root).
