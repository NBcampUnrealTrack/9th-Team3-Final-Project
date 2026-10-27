# Socket industrial presentation artwork

Generated with the built-in `image_gen` tool on 2026-10-27, using the user's approved socket mockup as a visual reference. These are decorative raster surfaces only. All names, icons, states, counts, money, descriptions and actions remain UMG widgets.

| Source | Cooked asset | Use |
| --- | --- | --- |
| `IndustrialCarrier.png` | `/Game/Knight/UI/Menu/Maintenance/Sockets/T_Socket_IndustrialCarrier` | Opaque socket-only backdrop with a circular apparatus and uninterrupted carrier. No baked slots or UI states. |
| `SocketBay.png` | `/Game/Knight/UI/Menu/Maintenance/Sockets/T_Socket_Bay` | Square slot background used by `BS_Socket_IndustrialSlot`. |

Both source PNGs are covered by `SourceArt/UI/**/*.png` in the repository's Git LFS rules. `Tools/Editor/style_socket_industrial.py` imports project-relative sources, assigns cooked references, and compiles affected widgets. It never changes gameplay definitions or saves.

The full-height side surfaces reuse `/Game/Knight/UI/Menu/Maintenance/Crafting/T_Crafting_IntegratedSide` without modifying that texture. Thin vertical rails use 2.5% horizontal slicing; the large native-resolution interior is downsampled rather than magnified from the old small card texture. Socket rows/actions reuse the existing Enhancement styles. Enhancement retains its original furnace art.

## Generation prompts

### IndustrialCarrier.png

Use case: stylized-concept. Asset type: production raster game UI machinery background, landscape 16:9 at high resolution. Create a text-free industrial socket-installation apparatus matching the dark worn gunmetal and restrained bronze/amber machinery in the attached approved socket mockup. This is decorative art to sit BEHIND live Unreal widgets, NOT a mockup or screenshot UI. Composition: frontal orthographic, symmetrical, dark metal industrial wall covering the full image. Center a large circular mechanical ring at 50% horizontal, 43% vertical, outer diameter about 70% of image height. Controlled thin amber lamps inside the ring, solid dark recessed metal disc at center. A single broad horizontal dark metal carrier crosses the disc centered at 43% vertical, occupying about 31% of image height and 44% of image width, with bolts/hinges only at the far ends. IMPORTANT the entire middle of the carrier is uninterrupted dark worn metal: NO socket holes, NO inset squares, NO symbols; three actual interactive square widgets will be placed over it later. Leave lower center below 80% image height quiet dark charcoal for actual action buttons. Outer 24% on each side should be dark subtle metal wall/pipes, without side-panel frames (panels are separate live widgets). Crisp plausible raster surfaces, fine subtle metal grain, restrained amber illumination, no smoke or sparks. Absolutely NO text, labels, numbers, money, UI icons, padlocks, input hints, weapons, characters, logos, grids, wireframes or interface buttons. Follow reference Image 1 ONLY for industrial material palette and ring/carrier concept; do not copy any interface text or state. Do not use enhancement screenshot as background.

### SocketBay.png

Use case: stylized-concept. Asset type: one production raster UMG square socket button background, 1024x1024. Match the worn dark metal and restrained bronze industrial apparatus just generated. A single front-facing SQUARE recessed socket bay, filling the square image edge to edge, with clean thin sculpted beveled steel rim about 6 percent of width, subtle bronze edge catches, worn corners and tiny screws, and a large dark clean charcoal interior about 85 percent width. Interior nearly uniform subtle fine-grain dark metal for readable live text and socket icons. Texture must be crisply resolved. No glowing selection state: live UI tint will control it. No perspective tilt. No secondary frames, no overlapping panels. Entire interior opaque. NO text, letters, numbers, icon, padlock, item, symbol, arrow, or socket module. Only one empty square framed background texture. Do not bake UI state into artwork. No translucent glow or noise over center.
