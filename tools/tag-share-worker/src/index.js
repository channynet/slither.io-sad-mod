// Slither Native tag sharing relay (Cloudflare Worker + Durable Objects, free plan).
//
// Clients playing on the same game server tell the relay which of their snakes wears what tag and
// learn the same of everyone else there:
//   POST /v1/room?s=<game server>   {"sid": <snake id or -1>, "tag": [...] or null}
//        -> {"p": [[sid, "ntl", id] | [sid, "img", hash, size, gap, angle, sway], ...]}
//      tag: ["ntl", id]  or  ["img", hash, size, gap, angle, sway]   (hash: FNV-1a 64 of the PNG, 16 hex)
//      An entry lasts 12 s; clients send theirs every few seconds while playing.
//   PUT  /v1/img/<hash>   the PNG (at most 120 KB; its hash must match)
//   GET  /v1/img/<hash>   the PNG
// Nothing else is kept: no names, no accounts. Game servers are only used as room names.

const ENTRY_MS = 12000;
const MAX_IMAGE = 120 * 1024;
const MAX_ROOM = 400;

function fnv1a64(bytes) {
  let h = 0xcbf29ce484222325n;
  const prime = 0x100000001b3n, mask = 0xffffffffffffffffn;
  for (const b of bytes) { h ^= BigInt(b); h = (h * prime) & mask; }
  return h.toString(16).padStart(16, "0");
}
const json = (v, status = 200) => new Response(JSON.stringify(v), { status, headers: { "content-type": "application/json" } });
const num = (v, lo, hi, d) => (typeof v === "number" && isFinite(v) ? Math.min(hi, Math.max(lo, v)) : d);

function cleanTag(t) {
  if (!Array.isArray(t)) return null;
  if (t[0] === "ntl" && Number.isInteger(t[1]) && t[1] >= 0 && t[1] < 1000) return ["ntl", t[1]];
  if (t[0] === "img" && typeof t[1] === "string" && /^[0-9a-f]{16}$/.test(t[1]))
    return ["img", t[1], num(t[2], 24, 320, 128), num(t[3], 0, 400, 24), num(t[4], -180, 180, 0), t[5] ? 1 : 0];
  return null;
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === "/v1/room" && request.method === "POST") {
      const server = (url.searchParams.get("s") || "").slice(0, 200);
      if (!server) return json({ error: "no server" }, 400);
      const room = env.ROOMS.get(env.ROOMS.idFromName(server));
      return room.fetch("https://room/", { method: "POST", body: await request.text() });
    }
    const m = url.pathname.match(/^\/v1\/img\/([0-9a-f]{16})$/);
    if (m) {
      const images = env.IMAGES.get(env.IMAGES.idFromName("images"));
      if (request.method === "GET") {
        const r = await images.fetch("https://images/" + m[1]);
        if (r.status !== 200) return new Response("not found", { status: 404 });
        return new Response(r.body, { headers: { "content-type": "image/png", "cache-control": "public, max-age=31536000, immutable" } });
      }
      if (request.method === "PUT") {
        const body = new Uint8Array(await request.arrayBuffer());
        if (body.length < 8 || body.length > MAX_IMAGE) return json({ error: "size" }, 413);
        if (body[0] !== 0x89 || body[1] !== 0x50 || body[2] !== 0x4e || body[3] !== 0x47) return json({ error: "not png" }, 415);
        if (fnv1a64(body) !== m[1]) return json({ error: "hash" }, 400);
        return images.fetch("https://images/" + m[1], { method: "PUT", body });
      }
    }
    if (url.pathname === "/") return new Response("Slither Native tag sharing relay\n");
    return new Response("not found", { status: 404 });
  },
};

// One game server: who wears what (kept in memory; clients send theirs again every few seconds).
export class Room {
  constructor(state) { this.state = state; this.entries = new Map(); }
  async fetch(request) {
    let body = {};
    try { body = JSON.parse(await request.text()); } catch (e) { return json({ error: "json" }, 400); }
    const now = Date.now();
    for (const [sid, e] of this.entries) if (now - e.at > ENTRY_MS) this.entries.delete(sid);
    const sid = body.sid;
    if (Number.isInteger(sid) && sid >= 0 && sid < 65536) {
      const tag = cleanTag(body.tag);
      if (tag && (this.entries.has(sid) || this.entries.size < MAX_ROOM)) this.entries.set(sid, { tag, at: now });
      else if (!tag) this.entries.delete(sid);
    }
    const p = [];
    for (const [s, e] of this.entries) p.push([s, ...e.tag]);
    return json({ p });
  }
}

// The images, by hash (kept in the object's own storage).
export class Images {
  constructor(state) { this.state = state; }
  async fetch(request) {
    const hash = new URL(request.url).pathname.slice(1);
    if (request.method === "PUT") {
      if (!(await this.state.storage.get(hash))) await this.state.storage.put(hash, await request.arrayBuffer());
      return json({ ok: true });
    }
    const data = await this.state.storage.get(hash);
    return data ? new Response(data) : new Response("not found", { status: 404 });
  }
}
