# Product

## Register

product

## Users

AEC / VDC professionals — construction planners, BIM/VDC coordinators, and project
teams (the tool's lead is a VDC project lead replacing paid tools like CMBuilder,
Fuzor, Synchro Pro, and Twinmotion). They use Earth4D to **author and present 4D
construction sequencing**: linking model elements and equipment to a schedule and
watching/recording the build play out in real-world geospatial context. Context of
use is focused, expert, often on a large monitor while coordinating or presenting
to stakeholders — not casual or mobile-first.

## Product Purpose

Earth4D is a **4D construction-sequencing tool** that plays a project's schedule
over **Google Photorealistic 3D Tiles** of the real site. It exists to make
construction phasing legible — what gets built or demolished, in what order, by
which equipment — with far less friction and cost than incumbent AEC tools.
Deliberately focused on the **4D / phasing intent**, not photoreal rendering
(Twinmotion's lane). Success = a coordinator can import a model + schedule
(ideally straight from Navisworks TimeLiner), see the sequence animate on the real
site, adjust it (by hand or by chat), and export/record it for the team.

Two surfaces share one design language:
- the **React web app** (`src/`) — the original tool, and the **design reference**
  impeccable works in;
- the **Unreal Engine 5.8 plugin** (`unreal/`) — native **UMG/Slate** UI.
  impeccable does **not** author UMG; its web design + design system is the
  **spec** that gets hand-translated into Slate. Keep both visually in lockstep.

## Brand Personality

**Cinematic · dark · spatial.** Viewport-first: the 3D scene is the hero and the
chrome recedes around it. Precise, confident, engineered — it should read as
serious professional software a VDC team trusts, with the composure of a film/edit
suite rather than a dashboard. Quiet by default; color and motion are reserved for
meaning (task type, phase, what's animating right now).

## Anti-references

- **Not consumer/playful** — no rounded-bubbly toy UI, mascots, candy colors, or
  cutesy copy. This is professional construction software.
- **Not dated-enterprise AEC** — avoid the cluttered gray wall-of-toolbar-buttons
  look of legacy Autodesk/Bentley tools; no visual overwhelm or 100 icons.
- General AI-slop bans still apply (no cream/sand bg, gradient text, eyebrow
  kickers, identical card grids, hero-metric template).

## Design Principles

1. **Viewport-first.** The real-world 3D scene is the primary surface; panels frame
   it and never fight it. Dark chrome so the site reads.
2. **The timeline is the spine.** 4D is the point — the schedule/playback timeline
   is always-present and central, not a buried tab.
3. **Color carries meaning, not decoration.** Reserve saturated color for task
   type, stage/phase, and the "currently animating" state so the eye tracks the
   sequence; everything else is restrained dark neutrals.
4. **Dense but legible.** Pro tools show a lot; earn density with clear hierarchy,
   spacing rhythm, and contrast — never clutter.
5. **One language, two runtimes.** Every design decision must be expressible in
   both web (CSS) and Unreal **UMG/Slate**; prefer tokens/patterns that translate
   cleanly to Slate brushes, fonts, and colors.

## Accessibility & Inclusion

No formal WCAG target set. Sensible defaults: keep text contrast comfortably
legible on the dark chrome, and — given the heavy 4D animation — honor reduced
motion where the platform supports it. Don't rely on hue alone to distinguish
task types / phases (pair with label/shape) so the schedule stays readable.
