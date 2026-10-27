# Crafting metal panel

`MetalPanelCutout.png` is the transparent-silhouette Crafting variant of the existing
`../Enhancement/MetalPanel.png`. The original Enhancement artwork is unchanged.
The dark interior is opaque; only the exterior of the panel is transparent.

Created with the built-in image generation/editing tool (background-extraction).
Final prompt: "Make a faithful reusable RGBA cutout of this exact panel, front-facing,
square, with minimal transparent margin only outside the clipped outer corner
silhouette. Keep the dark worn metal interior completely opaque for readable UI
content. Preserve the existing straight edges, sculpted worn steel/bronze corners,
restrained amber lights and dark roughness. No text, icons, new machinery or labels.
Use antialiased exterior transparency and preserve frame thickness near 10–12 percent."

Imported by `Tools/Editor/style_crafting_industrial.py` into
`/Game/Knight/UI/Menu/Maintenance/Crafting/T_Crafting_MetalPanel`.
UMG uses a 0.12 nine-slice margin and a 256px UI resource to retain compact corners.
Buttons and list rows reuse `T_Enhancement_MetalButton` through dedicated Crafting
style assets. No Enhancement texture/style is modified by this script.

## Integrated full-height presentation (2026-10-27)

The current Crafting screen uses `IntegratedSideMetal.png` (887 x 1774) as two
29%-viewport-wide, full-height side surfaces. A clipped ScaleToFill container
scales the whole image uniformly; the surface grain is never nine-sliced. The
texture is UI/uncompressed, no mipmaps, never-stream, with no maximum-size cap.
A neutral 0.48 RGB tint modulates only the background Image (alpha remains 1);
text and controls retain their own colors. The central 42% contains no UI background. The old cutout asset above is retained
for preservation but is no longer assigned to the screen.

`RestrainedMetalButton.png` (2172 x 724) supplies dedicated Crafting controls and
rows, at a 1024px resource cap and a narrow (0.008, 0.025) nine-slice rim. Text,
icons, counts and actions are live UMG controls. Enhancement assets are untouched.

Current imports:
- `/Game/Knight/UI/Menu/Maintenance/Crafting/T_Crafting_IntegratedSide`
- `/Game/Knight/UI/Menu/Maintenance/Crafting/T_Crafting_RestrainedButton`

Both images were generated with the built-in imagegen tool, using
`SourceArt/UI/Enhancement/MetalPanel.png` as the material/style reference.
No labels or gameplay data are baked into either image. Source PNGs are covered
by the repository's `SourceArt/UI/**/*.png` Git LFS rule.

Final generation prompt (side surface):
> Create a production-ready raster game UI surface, NOT a screenshot or menu mockup. One tall vertical panel, aspect ratio 1:2, high resolution 1024x2048 or greater. Use the provided existing panel image as material/style reference only. Make a much simpler, restrained full-height side wall: charcoal-black worn metal interior with fine sharp subtle grain, faint natural scratches, even dark neutral lighting and ample uninterrupted flat readable space. Extremely slim worn bronze metal vertical trims along the absolute left and right edges, each only about 2 percent width, tiny recessed fasteners sparsely along those edge strips. Straight orthographic front view. Fully opaque, full bleed to all image edges, no exterior margin, no perspective, no shadow beyond the rectangle, no transparent background. Top and bottom continue the metal surface to the edge without an enclosing ornamental frame or heavy caps. No thick corner blocks, no glowing lights, no large dents, no diagonal graphic lines, no ornamental machinery, no decorative wireframe. Absolutely no text, labels, icons, numbers, slots, UI controls, robot or background scene. Dark interior must be detailed and sharp rather than blurry or flat gradients. This is an actual tall side-panel texture to downsample at 558x1080 while preserving grain.

Final generation prompt (button):
> Production raster UI button/list-row surface for a restrained industrial game, one horizontal rectangular blank button with aspect ratio 6:1, high resolution. Entire full-bleed image is the button, no surrounding margin or background. Very dark charcoal neutral metal, fine low-contrast sharp subtle wear, quiet recessed dark interior for white Korean text to be added later as live UI. A single extremely thin straight dull bronze/steel hairline rim, simple square corners; 98 percent quiet content surface. Orthographic view, even illumination, fully opaque. No text, numbers, labels, icons, symbols, arrows, screws, corner machinery, segmented panels, lights, glow, large scratches or ornate decoration. Matching worn charcoal and restrained bronze aesthetic of the supplied panel reference. This will be rendered using nine-slice brushes for real buttons and recipe rows. Do not create a screen/mockup.
