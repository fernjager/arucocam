#!/usr/bin/env python3
"""Generate the bearing geometry diagrams.

No third-party dependencies.  Produces:
  bearing-top-view.svg           compass-style convention, + right (reported)
  bearing-side-view.svg          horiz / vert / slant geometry
  bearing-compact.svg            small two-panel summary for the README top

The SVGs are the source of truth; render PNG previews with Chrome if wanted.
"""

from __future__ import annotations

import math
from pathlib import Path

W, H = 1200, 840
INK = "#0F172A"
BODY = "#334155"
MUTED = "#64748B"
FAINT = "#94A3B8"
LINE = "#E2E8F0"
PANEL = "#FFFFFF"
BG = "#F5F6F2"
GRID = "#EEF1EC"
BLUE = "#2563EB"
GREEN = "#15803D"
AMBER = "#B45309"
GREEN_BG = "#F0FDF4"
BLUE_BG = "#EFF6FF"

FONT = ("system-ui,-apple-system,'Segoe UI',Helvetica,Arial,sans-serif")
MONO = "ui-monospace,SFMono-Regular,Menlo,Consolas,'Liberation Mono',monospace"
# A serif italic theta is much easier to read than a monospace one.
THETA = ('<tspan font-family="Georgia, \'Times New Roman\', serif" '
         'font-style="italic">θ</tspan>')


def n(value: float) -> str:
    text = f"{value:.2f}".rstrip("0").rstrip(".")
    return text or "0"


def esc(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def pt(cx: float, cy: float, r: float, deg: float) -> tuple[float, float]:
    """Point on a circle.  0 deg = +x, 90 deg = up (screen y is inverted)."""
    a = math.radians(deg)
    return cx + r * math.cos(a), cy - r * math.sin(a)


def text(
    x: float,
    y: float,
    value: str,
    *,
    size: float = 13,
    fill: str = INK,
    weight: int = 400,
    anchor: str = "start",
    family: str = FONT,
    spacing: float | None = None,
    opacity: float | None = None,
    halo: bool = False,
    rotate: float | None = None,
    extra: str = "",
) -> str:
    attrs = [
        f'x="{n(x)}"',
        f'y="{n(y)}"',
        f'font-family="{family}"',
        f'font-size="{n(size)}"',
        f'fill="{fill}"',
        f'font-weight="{weight}"',
        f'text-anchor="{anchor}"',
    ]
    if spacing is not None:
        attrs.append(f'letter-spacing="{n(spacing)}"')
    if opacity is not None:
        attrs.append(f'opacity="{n(opacity)}"')
    if halo:
        attrs.append('stroke="#FFFFFF" stroke-width="5" stroke-linejoin="round" '
                     'paint-order="stroke"')
    if rotate is not None:
        attrs.append(f'transform="rotate({n(rotate)} {n(x)} {n(y)})"')
    if extra:
        attrs.append(extra)
    return f'<text {" ".join(attrs)}>{esc(value)}</text>'


def rich_text(x: float, y: float, inner: str, *, size: float = 15,
              fill: str = INK, family: str = MONO, weight: int = 600,
              anchor: str = "start", halo: bool = False) -> str:
    extra = ('stroke="#FFFFFF" stroke-width="5" stroke-linejoin="round" '
             'paint-order="stroke"') if halo else ""
    return (f'<text x="{n(x)}" y="{n(y)}" font-family="{family}" '
            f'font-size="{n(size)}" font-weight="{weight}" fill="{fill}" '
            f'text-anchor="{anchor}" {extra}>{inner}</text>')


def arc_path(cx: float, cy: float, r: float, a0: float, a1: float) -> str:
    p0 = pt(cx, cy, r, a0)
    p1 = pt(cx, cy, r, a1)
    large = 1 if abs(a1 - a0) > 180 else 0
    sweep = 1 if a1 < a0 else 0
    return (f'M {n(p0[0])} {n(p0[1])} A {n(r)} {n(r)} 0 {large} {sweep} '
            f'{n(p1[0])} {n(p1[1])}')


def wedge(cx: float, cy: float, r: float, a0: float, a1: float,
          fill: str, opacity: float) -> str:
    p0 = pt(cx, cy, r, a0)
    p1 = pt(cx, cy, r, a1)
    large = 1 if abs(a1 - a0) > 180 else 0
    sweep = 1 if a1 < a0 else 0
    return (f'<path d="M {n(cx)} {n(cy)} L {n(p0[0])} {n(p0[1])} '
            f'A {n(r)} {n(r)} 0 {large} {sweep} {n(p1[0])} {n(p1[1])} Z" '
            f'fill="{fill}" opacity="{n(opacity)}"/>')


def defs() -> str:
    markers = []
    for name, colour in (("ink", BODY), ("blue", BLUE), ("green", GREEN),
                         ("amber", AMBER), ("faint", FAINT)):
        markers.append(
            f'<marker id="ah-{name}" viewBox="0 0 10 10" refX="9.2" refY="5" '
            f'markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
            f'<path d="M 0 0 L 10 5 L 0 10 z" fill="{colour}"/></marker>')
    return (
        "<defs>"
        '<pattern id="grid" width="24" height="24" patternUnits="userSpaceOnUse">'
        f'<path d="M 24 0 H 0 V 24" fill="none" stroke="{GRID}" stroke-width="1"/>'
        "</pattern>"
        + "".join(markers) +
        "</defs>"
    )


def pill(x: float, y: float, w: float, label: str, colour: str,
         bg: str) -> str:
    return (
        f'<rect x="{n(x)}" y="{n(y)}" width="{n(w)}" height="26" rx="13" '
        f'fill="{bg}" stroke="{colour}" stroke-opacity="0.28"/>'
        + text(x + w / 2, y + 17.5, label, size=10, fill=colour, weight=700,
               anchor="middle", spacing=1.4)
    )


def card(x: float, y: float, w: float, h: float, title: str) -> str:
    return (
        f'<rect x="{n(x)}" y="{n(y)}" width="{n(w)}" height="{n(h)}" rx="14" '
        f'fill="{PANEL}" stroke="{LINE}"/>'
        + text(x + 18, y + 25, title, size=10, fill=MUTED, weight=700,
               spacing=1.4)
        + f'<path d="M {n(x + 18)} {n(y + 36)} H {n(x + w - 18)}" '
          f'stroke="{LINE}" stroke-width="1"/>'
    )


def panel(x: float, y: float, w: float, h: float) -> str:
    return (
        f'<rect x="{n(x)}" y="{n(y)}" width="{n(w)}" height="{n(h)}" rx="18" '
        f'fill="{PANEL}" stroke="{LINE}"/>'
        f'<rect x="{n(x + 1)}" y="{n(y + 1)}" width="{n(w - 2)}" height="{n(h - 2)}" '
        f'rx="17" fill="url(#grid)" opacity="0.55"/>'
    )


def arUco(cx: float, cy: float, side: float, *, ghost: bool = False,
          colour: str = "#111827") -> str:
    """A small decorative ArUco-style marker, centred on (cx, cy)."""
    half = side / 2
    x = cx - half
    y = cy - half
    border = side * 0.14
    inner = side - 2 * border
    cell = inner / 4
    bits = {(0, 0), (1, 2), (2, 0), (3, 1), (2, 3), (0, 2), (3, 3)}
    cells = []
    for row in range(4):
        for col in range(4):
            if (row, col) in bits:
                cells.append(
                    f'<rect x="{n(x + border + col * cell)}" '
                    f'y="{n(y + border + row * cell)}" '
                    f'width="{n(cell)}" height="{n(cell)}" fill="{colour}"/>')
    opacity = ' opacity="0.55"' if ghost else ""
    dash = ' stroke-dasharray="4 3"' if ghost else ""
    return (
        f'<g{opacity}>'
        f'<rect x="{n(x)}" y="{n(y)}" width="{n(side)}" height="{n(side)}" '
        f'rx="2" fill="#FFFFFF" stroke="{colour}" stroke-width="1.5"{dash}/>'
        f'<rect x="{n(x + border)}" y="{n(y + border)}" width="{n(inner)}" '
        f'height="{n(inner)}" fill="#FFFFFF"/>'
        + "".join(cells) +
        "</g>"
    )


def camera_top(cx: float, cy: float) -> str:
    """Camera seen from above, lens at (cx, cy), body behind/below."""
    return (
        f'<rect x="{n(cx - 52)}" y="{n(cy + 6)}" width="104" height="58" rx="12" '
        f'fill="#1E293B" stroke="#0F172A" stroke-width="1.5"/>'
        f'<rect x="{n(cx - 42)}" y="{n(cy + 14)}" width="84" height="42" rx="8" '
        f'fill="#334155"/>'
        f'<circle cx="{n(cx)}" cy="{n(cy)}" r="17" fill="#0B1220" '
        f'stroke="#475569" stroke-width="2"/>'
        f'<circle cx="{n(cx)}" cy="{n(cy)}" r="10" fill="#1E293B"/>'
        f'<circle cx="{n(cx - 3)}" cy="{n(cy - 3)}" r="3" fill="#64748B" '
        f'opacity="0.8"/>'
    )


def camera_side(cx: float, cy: float) -> str:
    """Camera in profile, lens at (cx, cy), body to the left."""
    return (
        f'<rect x="{n(cx - 88)}" y="{n(cy - 34)}" width="82" height="68" rx="11" '
        f'fill="#1E293B" stroke="#0F172A" stroke-width="1.5"/>'
        f'<rect x="{n(cx - 78)}" y="{n(cy - 24)}" width="62" height="48" rx="7" '
        f'fill="#334155"/>'
        f'<circle cx="{n(cx)}" cy="{n(cy)}" r="21" fill="#0B1220" '
        f'stroke="#475569" stroke-width="2"/>'
        f'<circle cx="{n(cx)}" cy="{n(cy)}" r="13" fill="#1E293B"/>'
        f'<circle cx="{n(cx - 4)}" cy="{n(cy - 4)}" r="4" fill="#64748B" '
        f'opacity="0.8"/>'
    )


def compass_rose(cx: float, cy: float, r: float) -> str:
    out = [
        f'<circle cx="{n(cx)}" cy="{n(cy)}" r="{n(r)}" fill="#F8FAFC" '
        f'stroke="#CBD5E1" stroke-width="1.5"/>',
    ]
    for deg in range(0, 360, 45):
        p0 = pt(cx, cy, r - (7 if deg % 90 == 0 else 4), deg)
        p1 = pt(cx, cy, r - 1, deg)
        out.append(f'<path d="M {n(p0[0])} {n(p0[1])} L {n(p1[0])} {n(p1[1])}" '
                   f'stroke="#94A3B8" stroke-width="1.4"/>')
    labels = (("N", 90), ("E", 0), ("S", 270), ("W", 180))
    for label, deg in labels:
        x, y = pt(cx, cy, r + 14, deg)
        out.append(text(x, y + 4, label, size=11, fill=BODY, weight=700,
                        anchor="middle"))
    out.append(
        f'<path d="{arc_path(cx, cy, r + 4, 90, 20)}" fill="none" '
        f'stroke="{GREEN}" stroke-width="2" marker-end="url(#ah-green)"/>')
    lx, ly = pt(cx, cy, r + 20, 42)
    out.append(text(lx + 6, ly + 4, "CW", size=9.5, fill=GREEN, weight=700))
    out.append(
        f'<circle cx="{n(cx)}" cy="{n(cy)}" r="2.6" fill="{BODY}"/>'
        f'<path d="M {n(cx)} {n(cy)} L {n(cx)} {n(cy - 16)}" '
        f'stroke="{BODY}" stroke-width="2"/>')
    return "".join(out)


def header(title: str, subtitle: str, badge: str, badge_colour: str,
           badge_bg: str) -> str:
    return (
        text(48, 54, title, size=29, fill=INK, weight=700, spacing=-0.6)
        + text(48, 84, subtitle, size=14.5, fill=MUTED, weight=400)
        + pill(1032, 34, 120, badge, badge_colour, badge_bg)
    )


def top_view() -> str:
    title = "Top view — compass-style convention (+ right)"
    subtitle = (
        "Reported bearing: positive clockwise (right) from the camera forward axis."
    )
    badge = "CURRENT"
    badge_colour = GREEN
    badge_bg = GREEN_BG

    ox, oy = 460, 690
    right_len, left_len = 440, 300
    right_deg, left_deg = 32, 24
    rm = pt(ox, oy, right_len, 90 - right_deg)
    lm = pt(ox, oy, left_len, 90 + left_deg)
    right_sign, left_sign = right_deg, -left_deg

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
           f'viewBox="0 0 {W} {H}" role="img" '
           f'aria-labelledby="title desc">',
           f'<title id="title">{esc(title)}</title>',
           '<desc id="desc">Plan view of a camera, its forward axis, and two '
           'markers at positive and negative bearings.</desc>',
           defs(),
           f'<rect width="{W}" height="{H}" fill="{BG}"/>',
           header(title, subtitle, badge, badge_colour, badge_bg),
           panel(40, 110, 860, 690)]

    # Sectors: green on the positive (right) side, amber on the negative side.
    out.append(wedge(ox, oy, right_len - 18, 90, 90 - right_deg, GREEN, 0.07))
    out.append(wedge(ox, oy, left_len - 18, 90, 90 + left_deg, AMBER, 0.06))

    # Forward axis / split.
    out.append(
        f'<path d="M {ox} {oy - 8} V 158" stroke="#9AA5B1" stroke-width="1.5" '
        f'stroke-dasharray="7 7"/>')
    out.append(text(460, 143, "FORWARD AXIS / SPLIT", size=10, fill=MUTED,
                    weight=700, anchor="middle", spacing=1.6))
    out.append(rich_text(460, 196, f"{THETA} = 0", size=13.5, fill=BODY,
                         family=FONT, weight=700, anchor="middle", halo=True))

    # Left / right labels at the top of the panel.
    out.append(text(300, 172, "LEFT  −", size=12.5, fill=AMBER, weight=800,
                    anchor="middle", spacing=1.8))
    out.append(text(630, 172, "RIGHT  +", size=12.5, fill=GREEN, weight=800,
                    anchor="middle", spacing=1.8))

    # Sight lines.
    out.append(f'<path d="M {ox} {oy} L {n(rm[0])} {n(rm[1])}" '
               f'stroke="{GREEN}" stroke-width="2.4"/>')
    out.append(f'<path d="M {ox} {oy} L {n(lm[0])} {n(lm[1])}" '
               f'stroke="{AMBER}" stroke-width="2.1"/>')

    # Angle arcs.
    out.append(f'<path d="{arc_path(ox, oy, 170, 90, 90 - right_deg)}" '
               f'fill="none" stroke="{GREEN}" stroke-width="2.2" '
               f'marker-end="url(#ah-green)"/>')
    out.append(f'<path d="{arc_path(ox, oy, 140, 90, 90 + left_deg)}" '
               f'fill="none" stroke="{AMBER}" stroke-width="2.1" '
               f'marker-end="url(#ah-amber)"/>')

    # Arc labels.
    rx, ry = pt(ox, oy, 205, 74)
    out.append(text(rx, ry + 5, f"{right_sign:+d}°", size=18,
                    fill=GREEN, weight=800, anchor="middle",
                    family=MONO, halo=True))
    lx, ly = pt(ox, oy, 178, 102)
    out.append(text(lx, ly + 5, f"{left_sign:+d}°", size=16.5,
                    fill=AMBER, weight=800, anchor="middle",
                    family=MONO, halo=True))

    # Markers.
    out.append(arUco(rm[0], rm[1], 54))
    out.append(text(rm[0] + 40, rm[1] - 8, "marker 7", size=12.5, fill=INK,
                    weight=700))
    out.append(text(rm[0] + 40, rm[1] + 10, "right of forward", size=10.5,
                    fill=MUTED))
    out.append(arUco(lm[0], lm[1], 46))
    out.append(text(lm[0] - 34, lm[1] - 7, "marker 8", size=12, fill=INK,
                    weight=700, anchor="end"))
    out.append(text(lm[0] - 34, lm[1] + 10, "left of forward", size=10.5,
                    fill=MUTED, anchor="end"))

    # Camera and origin.
    out.append(camera_top(ox, oy))
    out.append(f'<circle cx="{ox}" cy="{oy}" r="4" fill="#FFFFFF" '
               f'stroke="#0F172A" stroke-width="2"/>')
    out.append(text(ox, 776, "camera", size=12.5, fill=BODY, weight=700,
                    anchor="middle"))
    out.append(text(ox, 793, "origin at optical centre", size=10.5,
                    fill=MUTED, anchor="middle"))

    # Scale note.
    out.append(text(872, 782, "top view · not to scale", size=10.5,
                    fill=FAINT, anchor="end"))

    # ---- sidebar ---------------------------------------------------------
    sx, sw = 924, 236
    out.append(card(sx, 110, sw, 190, "COMPASS"))
    out.append(compass_rose(1042, 202, 50))
    out.append(text(1042, 286, "0° N · 90° E", size=10.5, fill=MUTED,
                    anchor="middle"))

    # Sign card.
    out.append(card(sx, 316, sw, 196, "SIGN CONVENTION"))
    out.append(f'<circle cx="{sx + 26}" cy="392" r="5.5" fill="{GREEN}"/>')
    out.append(text(sx + 40, 397, "+ right / clockwise", size=11.5, fill=BODY,
                    weight=600))
    out.append(f'<circle cx="{sx + 26}" cy="420" r="5.5" fill="{AMBER}"/>')
    out.append(text(sx + 40, 425, "− left / counter-clockwise", size=11.5,
                    fill=BODY, weight=600))
    out.append(f'<path d="M {sx + 18} 444 H {sx + sw - 18}" stroke="{LINE}"/>')
    out.append(text(sx + 18, 468, "Right-positive adds directly", size=11,
                    fill=MUTED))
    out.append(text(sx + 18, 484, "to a compass heading.", size=11,
                    fill=MUTED))
    out.append(rich_text(sx + 18, 504, 'y<tspan baseline-shift="sub" '
                      'font-size="8">left</tspan> = −horiz · sin ' + THETA,
                      size=10.5, fill=FAINT, weight=500))

    # Formula card.
    out.append(card(sx, 528, sw, 176, "ABSOLUTE BEARING"))
    out.append(rich_text(
        sx + 18, 582,
        'H<tspan baseline-shift="sub" font-size="9">target</tspan> = '
        'H<tspan baseline-shift="sub" font-size="9">camera</tspan> + ' + THETA,
        size=15.5, fill=INK, weight=700))
    out.append(rich_text(sx + 18, 614, "070° + 32° = 102°", size=12.5,
                         fill=GREEN, weight=600))
    out.append(text(sx + 18, 642, "H_camera: compass heading of", size=10.5,
                    fill=MUTED))
    out.append(text(sx + 18, 657, "the forward axis.", size=10.5,
                    fill=MUTED))
    out.append(text(sx + 18, 681, "Wrap the result to 0…360°.", size=10.5,
                    fill=FAINT))

    # Footnote.
    out.append(text(sx, 738, "θ = 0 is the camera forward axis,", size=10.5,
                    fill=MUTED))
    out.append(text(sx, 754, "normally the image centre.", size=10.5,
                    fill=MUTED))
    out.append(text(sx, 770, "Bearing range: −180°…+180°.", size=10.5,
                    fill=FAINT))
    out.append(text(sx, 790, "Top view · looking down.", size=10.5, fill=FAINT))

    out.append("</svg>")
    return "\n".join(out)


def side_view() -> str:
    title = "Side view — horiz and vert"
    subtitle = ("Both are measured from the camera's optical centre. horiz lies in "
                "the camera's horizontal plane; vert is positive up.")
    ox, oy = 200, 610
    mx, my = 720, 310
    rx, ry = 720, 610
    horiz, vert = mx - ox, oy - my

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
           f'viewBox="0 0 {W} {H}" role="img" aria-labelledby="title desc">',
           f'<title id="title">{esc(title)}</title>',
           '<desc id="desc">Side view of a camera, a marker, the horizontal '
           'distance, the vertical offset, and the line-of-sight distance.</desc>',
           defs(),
           f'<rect width="{W}" height="{H}" fill="{BG}"/>',
           header(title, subtitle, "SIDE VIEW", BLUE, BLUE_BG),
           panel(40, 110, 860, 690)]

    # Camera axes triad.
    out.append(f'<path d="M 92 200 H 146" stroke="{FAINT}" stroke-width="1.4" '
               f'marker-end="url(#ah-faint)"/>')
    out.append(f'<path d="M 92 200 V 146" stroke="{FAINT}" stroke-width="1.4" '
               f'marker-end="url(#ah-faint)"/>')
    out.append(text(150, 204, "x", size=11, fill=MUTED, weight=700))
    out.append(text(92, 138, "z", size=11, fill=MUTED, weight=700, anchor="middle"))
    out.append(text(92, 220, "camera axes", size=10, fill=FAINT))

    # Horizontal reference plane.
    out.append(f'<path d="M {ox} {oy} H 850" stroke="#94A3B8" stroke-width="1.5" '
               f'stroke-dasharray="7 6"/>')
    out.append(text(850, 592, "camera horizontal plane (z = 0)", size=11,
                    fill=MUTED, anchor="end"))

    # Negative example: a marker below the horizontal plane.  A vertical
    # drop is enough to show the sign; a sight line here would cross the
    # horiz dimension and clutter the main construction.
    gx, gy = 660, 730
    out.append(f'<path d="M {gx} {oy} V {gy - 12}" stroke="{AMBER}" '
               f'stroke-width="1.5" stroke-dasharray="4 4" opacity="0.8"/>')
    out.append(f'<circle cx="{gx}" cy="{oy}" r="2.6" fill="{AMBER}"/>')
    out.append(arUco(gx, gy, 40, ghost=True, colour=AMBER))
    out.append(text(gx + 30, gy - 4, "vert < 0", size=11.5, fill=AMBER,
                    weight=700))
    out.append(text(gx + 30, gy + 12, "below the plane", size=10, fill=AMBER,
                    opacity=0.85))

    # Line of sight.
    out.append(f'<path d="M {ox} {oy} L {mx} {my}" stroke="{BLUE}" '
               f'stroke-width="2.5"/>')
    lx, ly = ox + horiz * 0.5 - 9, oy - vert * 0.5 - 18
    angle = -math.degrees(math.atan2(vert, horiz))
    out.append(text(lx, ly, "slant = hypot(horiz, vert)", size=12.5, fill=BLUE,
                    weight=600, anchor="middle", rotate=angle, halo=True))

    # Elevation arc.
    out.append(f'<path d="{arc_path(ox, oy, 140, 0, math.degrees(math.atan2(vert, horiz)))}" '
               f'fill="none" stroke="{GREEN}" stroke-width="2" '
               f'marker-end="url(#ah-green)"/>')
    ax, ay = pt(ox, oy, 166, math.degrees(math.atan2(vert, horiz)) / 2)
    out.append(text(ax, ay + 5, "φ", size=18, fill=GREEN, weight=800,
                    anchor="middle", halo=True))

    # horiz dimension, set below the camera body so the origin arrow is visible.
    dim_y = oy + 40
    out.append(f'<path d="M {ox} {dim_y} H {rx}" stroke="{BODY}" '
               f'stroke-width="1.4" marker-start="url(#ah-ink)" '
               f'marker-end="url(#ah-ink)"/>')
    out.append(f'<path d="M {ox} {oy + 5} V {dim_y + 11} M {rx} {oy + 5} V {dim_y + 11}" '
               f'stroke="{LINE}" stroke-width="1"/>')
    out.append(text((ox + rx) / 2, dim_y + 30, "horiz", size=16, fill=INK,
                    weight=700, anchor="middle"))
    out.append(text((ox + rx) / 2, dim_y + 48, "in the camera's horizontal plane",
                    size=10.5, fill=MUTED, anchor="middle"))

    # vert dimension.
    vx = mx + 50
    out.append(f'<path d="M {vx} {my} V {oy}" stroke="{GREEN}" '
               f'stroke-width="1.4" marker-start="url(#ah-green)" '
               f'marker-end="url(#ah-green)"/>')
    out.append(f'<path d="M {mx + 5} {my} H {vx + 11} M {mx + 5} {oy} H {vx + 11}" '
               f'stroke="#BBF7D0" stroke-width="1"/>')
    out.append(text(vx + 14, (my + oy) / 2 - 4, "vert", size=16, fill=GREEN,
                    weight=700))
    out.append(text(vx + 14, (my + oy) / 2 + 14, "positive up", size=10.5,
                    fill=GREEN))

    # Marker and foot.
    out.append(arUco(mx, my, 64))
    out.append(text(mx, my - 46, "marker 7", size=13, fill=INK, weight=700,
                    anchor="middle"))
    out.append(f'<circle cx="{rx}" cy="{ry}" r="3.5" fill="{BODY}"/>')

    # Camera and origin.
    out.append(camera_side(ox, oy))
    out.append(f'<circle cx="{ox}" cy="{oy}" r="4.5" fill="#FFFFFF" '
               f'stroke="{INK}" stroke-width="2"/>')
    out.append(text(ox - 28, oy + 80, "camera", size=12.5, fill=BODY,
                    weight=700, anchor="middle"))
    out.append(text(ox - 28, oy + 96, "origin at optical centre", size=10,
                    fill=MUTED, anchor="middle"))

    out.append(text(872, 782, "side view · not to scale", size=10.5,
                    fill=FAINT, anchor="end"))

    # ---- sidebar ---------------------------------------------------------
    sx, sw = 924, 236
    out.append(card(sx, 110, sw, 232, "MEASUREMENTS"))
    rows = (
        ("horiz", INK, "horizontal distance in", "the camera's horizontal plane"),
        ("vert", GREEN, "vertical offset from the", "camera; positive up"),
        ("slant", BLUE, "true 3D distance;", "hypot(horiz, vert)"),
    )
    for index, (name, colour, line1, line2) in enumerate(rows):
        top = 166 + index * 58
        out.append(f'<rect x="{sx + 18}" y="{top}" width="4" height="38" '
                   f'rx="2" fill="{colour}"/>')
        out.append(rich_text(sx + 34, top + 15, name, size=13, fill=colour,
                             weight=700))
        out.append(text(sx + 34, top + 32, line1, size=10.5, fill=MUTED))
        out.append(text(sx + 34, top + 46, line2, size=10.5, fill=MUTED))

    out.append(card(sx, 358, sw, 190, "RELATIONSHIPS"))
    formulas = (
        "slant = hypot(horiz, vert)",
        "φ = atan2(vert, horiz)",
        "vert = slant · sin φ",
        "horiz = slant · cos φ",
    )
    for index, formula in enumerate(formulas):
        out.append(rich_text(sx + 18, 414 + index * 29, formula, size=11.5,
                             fill=BODY, weight=600))
    out.append(text(sx + 18, 532, "φ is elevation above the plane.", size=10,
                    fill=FAINT))

    out.append(card(sx, 564, sw, 150, "NOTE"))
    notes = (
        "vert is not height above the floor.",
        "horiz is not the straight-line",
        "distance.",
        "Origin: the camera's optical centre.",
    )
    for index, note in enumerate(notes):
        out.append(text(sx + 18, 614 + index * 21, note, size=10.5,
                        fill=MUTED))

    out.append(text(sx, 744, "The true 3D distance is the", size=10.5,
                    fill=FAINT))
    out.append(text(sx, 760, "line-of-sight length.", size=10.5, fill=FAINT))

    out.append("</svg>")
    return "\n".join(out)


def compact_view() -> str:
    """A small two-panel summary for the top of README.md."""
    cw, ch = 1200, 470
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{cw}" height="{ch}" '
           f'viewBox="0 0 {cw} {ch}" role="img" '
           f'aria-labelledby="title desc">',
           '<title id="title">Camera-relative bearing, horiz, and vert</title>',
           '<desc id="desc">Compact top view of the camera bearing and side view '
           'of the horizontal, vertical, and slant distances.</desc>',
           defs(),
           f'<rect width="{cw}" height="{ch}" fill="{BG}"/>',
           panel(20, 20, 570, 430),
           panel(610, 20, 570, 430)]

    # ---- left: top view --------------------------------------------------
    out.append(text(44, 52, "TOP VIEW", size=10, fill=MUTED, weight=700,
                    spacing=1.6))
    out.append(text(44, 80, "bearing  (+ right)", size=18, fill=INK,
                    weight=700))
    out.append(rich_text(44, 105, "H<tspan baseline-shift=\"sub\" "
                      "font-size=\"8\">target</tspan> = "
                      "H<tspan baseline-shift=\"sub\" font-size=\"8\">"
                      "camera</tspan> + " + THETA,
                      size=11.5, fill=MUTED, weight=600))

    ox, oy = 300, 360
    rm = pt(ox, oy, 210, 90 - 32)
    lm = pt(ox, oy, 150, 90 + 24)
    out.append(wedge(ox, oy, 190, 90, 58, GREEN, 0.08))
    out.append(wedge(ox, oy, 132, 90, 114, AMBER, 0.06))
    out.append(f'<path d="M {ox} {oy - 6} V 130" stroke="#9AA5B1" '
               f'stroke-width="1.4" stroke-dasharray="6 6"/>')
    out.append(text(182, 148, "LEFT  −", size=10.5, fill=AMBER, weight=800,
                    anchor="middle", spacing=1.4))
    out.append(text(418, 148, "RIGHT  +", size=10.5, fill=GREEN, weight=800,
                    anchor="middle", spacing=1.4))
    out.append(rich_text(ox, 170, f"{THETA} = 0", size=11, fill=BODY,
                         family=FONT, weight=700, anchor="middle", halo=True))
    out.append(f'<path d="M {ox} {oy} L {n(rm[0])} {n(rm[1])}" '
               f'stroke="{GREEN}" stroke-width="2.2"/>')
    out.append(f'<path d="M {ox} {oy} L {n(lm[0])} {n(lm[1])}" '
               f'stroke="{AMBER}" stroke-width="1.8"/>')
    out.append(f'<path d="{arc_path(ox, oy, 96, 90, 58)}" fill="none" '
               f'stroke="{GREEN}" stroke-width="2" marker-end="url(#ah-green)"/>')
    out.append(f'<path d="{arc_path(ox, oy, 76, 90, 114)}" fill="none" '
               f'stroke="{AMBER}" stroke-width="1.8" marker-end="url(#ah-amber)"/>')
    ax, ay = pt(ox, oy, 126, 74)
    out.append(text(ax, ay + 4, "+32°", size=14, fill=GREEN, weight=800,
                    anchor="middle", family=MONO, halo=True))
    lx, ly = pt(ox, oy, 105, 102)
    out.append(text(lx, ly + 4, "−24°", size=12.5, fill=AMBER, weight=800,
                    anchor="middle", family=MONO, halo=True))
    out.append(arUco(rm[0], rm[1], 42))
    out.append(text(rm[0] + 30, rm[1] + 4, "marker", size=10.5, fill=INK,
                    weight=700))
    out.append(arUco(lm[0], lm[1], 32))
    out.append(camera_top(ox, oy))
    out.append(f'<circle cx="{ox}" cy="{oy}" r="3.5" fill="#FFFFFF" '
               f'stroke="{INK}" stroke-width="1.8"/>')
    out.append(text(ox, 440, "camera", size=10.5, fill=BODY, weight=700,
                    anchor="middle"))

    # ---- right: side view ------------------------------------------------
    out.append(text(634, 52, "SIDE VIEW", size=10, fill=MUTED, weight=700,
                    spacing=1.6))
    out.append(text(634, 80, "horiz / vert", size=18, fill=INK, weight=700))
    out.append(rich_text(634, 105, "slant = hypot(horiz, vert)", size=11.5,
                         fill=MUTED, weight=600))

    ox, oy = 720, 360
    mx, my = 1050, 190
    out.append(f'<path d="M {ox} {oy} H 1160" stroke="#94A3B8" '
               f'stroke-width="1.3" stroke-dasharray="6 6"/>')
    out.append(f'<path d="M {ox} {oy} L {mx} {my}" stroke="{BLUE}" '
               f'stroke-width="2.2"/>')
    out.append(f'<path d="M {ox} {oy + 22} H {mx}" stroke="{BODY}" '
               f'stroke-width="1.2" marker-start="url(#ah-ink)" '
               f'marker-end="url(#ah-ink)"/>')
    out.append(f'<path d="M {mx + 36} {my} V {oy}" stroke="{GREEN}" '
               f'stroke-width="1.2" marker-start="url(#ah-green)" '
               f'marker-end="url(#ah-green)"/>')
    out.append(text((ox + mx) / 2, oy + 43, "horiz", size=13.5, fill=INK,
                    weight=700, anchor="middle"))
    out.append(text(mx + 48, (my + oy) / 2 + 4, "vert", size=13.5, fill=GREEN,
                    weight=700))
    lx, ly = (ox + mx) / 2 - 4, (oy + my) / 2 - 12
    out.append(text(lx, ly, "slant", size=12, fill=BLUE, weight=600,
                    anchor="middle", rotate=-26.6, halo=True))
    out.append(f'<path d="{arc_path(ox, oy, 76, 0, 26.6)}" fill="none" '
               f'stroke="{GREEN}" stroke-width="1.8" marker-end="url(#ah-green)"/>')
    ax, ay = pt(ox, oy, 98, 13.3)
    out.append(text(ax, ay + 4, "φ", size=15, fill=GREEN, weight=800,
                    anchor="middle", halo=True))
    out.append(arUco(mx, my, 44))
    out.append(text(mx, my - 32, "marker", size=10.5, fill=INK, weight=700,
                    anchor="middle"))
    out.append(camera_side(ox, oy))
    out.append(f'<circle cx="{ox}" cy="{oy}" r="3.5" fill="#FFFFFF" '
               f'stroke="{INK}" stroke-width="1.8"/>')
    out.append(text(ox, 440, "camera", size=10.5, fill=BODY, weight=700,
                    anchor="middle"))
    out.append(text(1150, 440, "not to scale", size=9.5, fill=FAINT,
                    anchor="end"))

    out.append("</svg>")
    return "\n".join(out)


def main() -> None:
    here = Path(__file__).resolve().parent
    outputs = {
        "bearing-top-view.svg": top_view(),
        "bearing-side-view.svg": side_view(),
        "bearing-compact.svg": compact_view(),
    }
    for name, svg in outputs.items():
        (here / name).write_text(svg + "\n", encoding="utf-8")
        print(f"wrote {name}")


if __name__ == "__main__":
    main()
