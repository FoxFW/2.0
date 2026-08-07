/*
 * GitHub Profile plugin - "trading card" badge.
 *
 * Tuned for the 208x112 tag (the common case); scales to any size.
 *
 *   ┌─────────────────────────────────────────┐
 *   │██████████████ @torvalds █████████████████│  accent stripe + knockout
 *   │  ╭──────╮                                │
 *   │  │AVATAR│   LINUS TORVALDS               │  scale-2 name
 *   │  │ round│   Creator of Linux             │  optional 1-line bio
 *   │  ╰──────╯                                │
 *   │ ──────────────────────────────────────── │
 *   │   178K     │    234     │     712        │
 *   │   STARS    │ FOLLOWERS  │    REPOS       │
 *   └─────────────────────────────────────────┘
 *
 * Palette:
 *   - Accent: top stripe + the STARS number.
 *   - Black: typography, frame, dividers, dithered avatar.
 *   - White: paper, knockout @handle in the stripe.
 *
 * Data: api.github.com only - the heatmap API was the slowest leg of
 * the previous render and the trading-card layout has no heatmap, so
 * we drop that fetch entirely. Render p99 went from ~1.4s to ~600ms.
 */

import { Canvas, Ink } from "../canvas";
import { Plugin, AccentMode } from "../plugin";
import { fetchImageGray, blitGrayDither } from "../image_util";

interface GhUser {
  login: string;
  name: string | null;
  bio: string | null;
  avatar_url: string;
  followers: number;
  public_repos: number;
}

interface GhRepoLite { stargazers_count: number; }

async function fetchUser(login: string): Promise<GhUser | null> {
  try {
    const r = await fetch(`https://api.github.com/users/${encodeURIComponent(login)}`, {
      headers: { "User-Agent": "TagTinker/1.0", "Accept": "application/vnd.github+json" },
    });
    if (!r.ok) return null;
    return await r.json() as GhUser;
  } catch { return null; }
}

async function fetchTotalStars(login: string): Promise<number | null> {
  try {
    const r = await fetch(
      `https://api.github.com/users/${encodeURIComponent(login)}/repos` +
      `?per_page=100&sort=pushed`,
      { headers: { "User-Agent": "TagTinker/1.0", "Accept": "application/vnd.github+json" } });
    if (!r.ok) return null;
    const repos: GhRepoLite[] = await r.json();
    let total = 0;
    for (const r of repos) total += r.stargazers_count | 0;
    return total;
  } catch { return null; }
}

function compact(n: number): string {
  if (n >= 1_000_000) return (n / 1_000_000).toFixed(n < 10_000_000 ? 1 : 0) + "M";
  if (n >= 1_000)     return (n / 1_000).toFixed(n < 10_000 ? 1 : 0) + "K";
  return String(n);
}

function ascii(s: string): string {
  return s
    .replace(/[\u2018\u2019\u02BC]/g, "'")
    .replace(/[\u201C\u201D]/g, '"')
    .replace(/[\u2013\u2014]/g, "-")
    .replace(/\u2026/g, "...")
    .replace(/[^\x20-\x7E]/g, "");
}

function truncate(s: string, max: number): string {
  if (max <= 0) return "";
  if (s.length <= max) return s;
  if (max <= 3) return s.slice(0, max);
  return s.slice(0, max - 3) + "...";
}

/* Deterministic filler lines for users with no bio. The choice is
 * keyed on the login so the same person always gets the same quip
 * across renders (otherwise it'd flicker between each refresh and
 * read as a glitch rather than a feature). Kept short so it fits
 * on one line at the 208x112 column width (~22 chars). */
const BIO_FILLERS: string[] = [
  "no bio. just vibes.",
  "git push origin self",
  "probably refactoring.",
  "404: bio not found.",
  "lurking in repos.",
  "shipping, not updating.",
  "lives in the terminal.",
  "touch grass? maybe later.",
  "compiling thoughts...",
  "reads code for fun.",
  "bio loading...",
  "rm -rf /writers-block",
];

function pickFiller(login: string): string {
  let h = 0;
  for (let i = 0; i < login.length; i++) h = (h * 31 + login.charCodeAt(i)) | 0;
  return BIO_FILLERS[Math.abs(h) % BIO_FILLERS.length];
}

/** Greedy word-wrap into at most `maxLines` lines of `maxChars`.
 *  Words that don't fit the current line are bumped to the next
 *  line (never split mid-word). Anything that doesn't fit in the
 *  vertical budget is dropped silently - no ellipsis, no mid-word
 *  cut, the bio just stops on a clean word break. */
function wrapText(s: string, maxChars: number, maxLines: number): string[] {
  if (maxChars <= 0 || maxLines <= 0) return [];
  const words = s.split(/\s+/).filter(Boolean);
  const lines: string[] = [];
  let cur = "";
  for (let i = 0; i < words.length && lines.length < maxLines; i++) {
    const w = words[i];
    const cand = cur ? cur + " " + w : w;
    if (cand.length <= maxChars) {
      cur = cand;
    } else {
      if (cur) {
        lines.push(cur);
        if (lines.length >= maxLines) { cur = ""; break; }
      }
      /* If a single word is wider than the column we still need to
       * place it somewhere - otherwise the bio could go entirely
       * blank for a user whose first word is e.g. an absurdly long
       * URL. Hard-break it on column boundaries (rare path). */
      if (w.length <= maxChars) {
        cur = w;
      } else {
        let rest = w;
        while (rest.length > maxChars && lines.length < maxLines) {
          lines.push(rest.slice(0, maxChars));
          rest = rest.slice(maxChars);
        }
        cur = lines.length < maxLines ? rest : "";
      }
    }
  }
  if (cur && lines.length < maxLines) lines.push(cur);
  return lines;
}

export const githubPlugin: Plugin = {
  manifest: {
    id: "github",
    name: "GitHub Profile",
    description: "Trading-card style profile badge with stars/followers/repos",
    accent_modes: 1 | 2 | 4,
    params: [
      { key: "username", label: "Username", type: "string", default: "torvalds" },
    ],
  },

  async render(params, W, H, accent: AccentMode) {
    const login = ((params.username ?? "torvalds").trim() || "torvalds");

    const planes: 1 | 2 = accent === "none" ? 1 : 2;
    const c = new Canvas(W, H, planes);
    const acc: Ink = planes === 2 ? 1 : 0;
    const blk: Ink = 0;

    const [user, stars] = await Promise.all([
      fetchUser(login),
      fetchTotalStars(login),
    ]);

    if (!user) {
      const t1 = "USER NOT FOUND";
      const t2 = "@" + login;
      const s1 = c.textSize(t1, 2), s2 = c.textSize(t2, 1);
      c.drawText((W - s1.w) >> 1, (H >> 1) - s1.h, t1, blk, 2);
      c.drawText((W - s2.w) >> 1, (H >> 1) + 2,    t2, blk, 1);
      c.rect(0, 0, W, H, blk);
      return c;
    }

    /* ── Geometry ──────────────────────────────────────────────
     * Three horizontal bands:
     *   1. stripe   - thin accent bar with knockout @handle
     *   2. portrait - circular avatar + display name + (optional bio)
     *   3. stats    - 3-column number panel (stars / followers / repos)
     * Coordinates are derived from W/H so a 296x128 tag gets a roomier
     * version of the same card for free, but everything is sized so
     * the 208x112 layout stays tight and balanced. */
    const stripeH = 11;
    const padX = 4;
    /* Stats band height = scale-2 number (14) + 2-px gap + scale-1
     * label (7) + 4-px breathing room top/bot = 31 px. We push the
     * divider as far down as that constraint allows so the avatar
     * gets the rest of the page. On 112-tall it lands at ~78. */
    const statsBandH = 7 + 2 + 14 + 4 + 4;
    const dividerY = H - statsBandH;

    /* ── 1. Top accent stripe with knockout handle ─────────────
     * fillRect first (accent ink), then drawTextWhite punches the
     * @handle out of it, leaving paper showing through the letters.
     * Looks like a printed magazine title bar. */
    c.fillRect(0, 0, W, stripeH, acc);
    /* Show the full short URL ("github.com/<login>") - reads as a
     * proper profile link instead of just an @handle. If the login
     * is too long for the column we drop back to "@<login>". */
    const fullLink = "github.com/" + ascii(user.login);
    const linkMax  = Math.max(1, Math.floor((W - 12) / 6));
    const linkStr  = fullLink.length <= linkMax
      ? fullLink
      : truncate("@" + ascii(user.login), linkMax);
    const linkSize = c.textSize(linkStr, 1);
    c.drawTextWhite((W - linkSize.w) >> 1, (stripeH - 7) >> 1, linkStr, 1);

    /* ── 2. Portrait band: square avatar + name + bio ─────────
     * Avatar is a clean square so it gets the full pixel budget
     * for the dithered face (no corners wasted to a circle mask).
     * Size scales with the band height so a wider tag gets a
     * bigger picture automatically. */
    const bandTop = stripeH + 2;
    const bandBot = dividerY - 2;
    const bandH   = bandBot - bandTop;
    const avSize  = Math.min(64, bandH);            // 60 on 112-tall
    const avX     = padX + 4;
    const avY     = bandTop + ((bandH - avSize) >> 1);

    const avatarUrl = user.avatar_url.includes("?")
      ? `${user.avatar_url}&s=${avSize * 2}`
      : `${user.avatar_url}?s=${avSize * 2}`;
    const avatar = await fetchImageGray(avatarUrl);
    if (avatar) {
      blitGrayDither(c, avX + 1, avY + 1, avSize - 2, avSize - 2, avatar, blk);
    } else {
      const ini = (user.login.charAt(0) || "?").toUpperCase();
      const sz = c.textSize(ini, 4);
      c.drawText(avX + ((avSize - sz.w) >> 1),
                 avY + ((avSize - sz.h) >> 1), ini, blk, 4);
    }
    /* 1-px square frame so the dithered face has a clean edge. */
    c.rect(avX, avY, avSize, avSize, blk);

    /* Name + (optional) bio, right of the avatar. */
    const tX = avX + avSize + 8;
    const tW = W - tX - padX;
    const nameRaw = (user.name && user.name.trim() ? user.name : user.login).trim();
    const nameAscii = ascii(nameRaw).toUpperCase();
    /* Prefer scale 2; drop to scale 1 if it doesn't fit. Bold is
     * faux'd by drawing the glyphs twice with a 1-px x-offset -
     * a classic bitmap-font trick that keeps the 5x7 readable
     * while giving the name visible weight. */
    const max2 = Math.max(1, Math.floor(tW / 12));
    const max1 = Math.max(1, Math.floor(tW / 6));
    const nameScale = c.textSize(nameAscii, 2).w <= tW && nameAscii.length <= max2 ? 2 : 1;
    const nameStr = truncate(nameAscii, nameScale === 2 ? max2 : max1);
    const nameH   = 7 * nameScale;
    /* Anchor the name to the top of the avatar (visually aligned
     * with the picture) and let the bio flow downward. */
    const blockTop = avY + 1;
    c.drawText(tX,     blockTop, nameStr, blk, nameScale);
    c.drawText(tX + 1, blockTop, nameStr, blk, nameScale);   // faux-bold

    /* ── Flexible bio ──────────────────────────────────────────
     * Wrap into however many lines fit between the name and the
     * divider. Lines that don't fit get an ellipsis on the last
     * surviving line, so the user can tell text was truncated.
     * If the user has no bio at all we fall back to a deterministic
     * filler line so the slot doesn't read as a render bug. */
    {
      const bioText = user.bio && user.bio.trim()
        ? ascii(user.bio).trim()
        : pickFiller(user.login);
      const bioTop    = blockTop + nameH + 3;
      const bioBudget = bandBot - bioTop;
      /* Glyphs are 7 px tall - flush them with no inter-line gap so
       * we squeeze the maximum number of full lines into the band.
       * Descenders aren't a concern in a 5x7 bitmap font. */
      const lineH     = 7;
      const maxLines  = Math.max(0, Math.floor((bioBudget + 1) / lineH));
      if (maxLines > 0) {
        const lines = wrapText(bioText, max1, maxLines);
        let by = bioTop;
        for (const ln of lines) {
          c.drawText(tX, by, ln, blk, 1);
          by += lineH;
        }
      }
    }

    /* ── 3. Hairline divider ───────────────────────────────────
     * Short of the side margins so it reads as a rule, not a
     * frame extension. */
    c.hline(padX + 4, dividerY, W - 2 * (padX + 4), blk);

    /* ── 4. Stats panel: 3 equal columns ───────────────────────
     * Numbers in scale 2, labels in scale 1, vertical hairlines
     * between columns. The STARS number gets accent ink so the
     * eye lands on it first. */
    const statsTop = dividerY + 2;
    const statsBot = H - 2;
    const colW     = Math.floor((W - 2 * padX) / 3);
    const stats: Array<{ num: string; label: string; accent: boolean }> = [
      { num: compact(stars ?? 0),         label: "STARS",     accent: true  },
      { num: compact(user.followers),     label: "FOLLOWERS", accent: false },
      { num: compact(user.public_repos),  label: "REPOS",     accent: false },
    ];
    /* Number sits directly above the label with a 2-px gap, both
     * pinned to the bottom of the band. This keeps the divider /
     * portrait band tall while the stats hug the bottom edge. */
    const lblY = statsBot - 8;
    const numY = lblY - 14 - 2;
    for (let i = 0; i < 3; i++) {
      const s = stats[i];
      const cx = padX + colW * i + (colW >> 1);
      const ns = c.textSize(s.num, 2);
      c.drawText(cx - (ns.w >> 1), numY, s.num, s.accent ? acc : blk, 2);
      const ls = c.textSize(s.label, 1);
      c.drawText(cx - (ls.w >> 1), lblY, s.label, blk, 1);
    }
    /* Column dividers: a couple of pixels in from the band edges
     * so they don't kiss the outer frame. */
    for (let i = 1; i < 3; i++) {
      const dx = padX + colW * i;
      c.vline(dx, statsTop + 2, statsBot - statsTop - 4, blk);
    }

    /* ── Outer 1-px frame ──────────────────────────────────────
     * Drawn last so it overlays the stripe corners cleanly. */
    c.rect(0, 0, W, H, blk);
    return c;
  },
};
