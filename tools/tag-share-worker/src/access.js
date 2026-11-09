// Client access control is independent of tag sharing. No administrator secret goes in the client.
import { adminPage } from "./admin.js";

const DAY = 86400000;
export const json = (body, status = 200) => new Response(JSON.stringify(body), {
  status, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
});

export function cleanIP(value) {
  if (typeof value !== "string" || value.length > 45) return null;
  if (/^\d{1,3}(\.\d{1,3}){3}$/.test(value)) {
    const parts = value.split(".");
    return parts.every(p => String(Number(p)) === p && Number(p) <= 255) ? value : null;
  }
  if (!/^[0-9a-fA-F:.]+$/.test(value) || !value.includes(":")) return null;
  try { return new URL("http://[" + value + "]/").hostname.slice(1, -1); } catch { return null; }
}

async function bodyJSON(request) {
  const reader = request.body?.getReader();
  if (!reader) throw new Error("body");
  let text = "", length = 0;
  const decoder = new TextDecoder();
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      length += value.length;
      if (length > 512) throw new Error("size");
      text += decoder.decode(value, { stream: true });
    }
    return JSON.parse(text + decoder.decode());
  } finally { await reader.cancel(); }
}

async function authorized(request, secret) {
  if (typeof secret !== "string" || secret.length < 32) return false;
  const supplied = request.headers.get("authorization") || "";
  if (!supplied.startsWith("Bearer ") || supplied.length > 256) return false;
  const encoder = new TextEncoder();
  const a = new Uint8Array(await crypto.subtle.digest("SHA-256", encoder.encode(supplied.slice(7))));
  const b = new Uint8Array(await crypto.subtle.digest("SHA-256", encoder.encode(secret)));
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a[i] ^ b[i];
  return diff === 0;
}

export async function accessFetch(request, env) {
  const url = new URL(request.url);
  if (url.pathname === "/admin" && request.method === "GET") {
    return new Response(adminPage, { headers: {
      "content-type": "text/html; charset=utf-8", "cache-control": "no-store",
      "content-security-policy": "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'",
      "referrer-policy": "no-referrer", "x-content-type-options": "nosniff",
    } });
  }
  if (!env.ACCESS || typeof env.ADMIN_TOKEN !== "string" || env.ADMIN_TOKEN.length < 32)
    return json({ error: "access service not configured" }, 503);
  const store = env.ACCESS.get(env.ACCESS.idFromName("client-access-v1"));
  if (url.pathname === "/v1/access" && request.method === "POST") {
    const ip = cleanIP(request.headers.get("CF-Connecting-IP"));
    if (!ip) return json({ error: "client IP unavailable" }, 503);
    let input;
    try { input = await bodyJSON(request); } catch { return json({ error: "invalid body" }, 400); }
    if (!input || typeof input.version !== "string" || !/^[A-Za-z0-9._-]{1,48}$/.test(input.version))
      return json({ error: "invalid version" }, 400);
    const cf = request.cf || {};
    return store.fetch("https://access/check", { method: "POST", body: JSON.stringify({
      ip, version: input.version,
      country: typeof cf.country === "string" ? cf.country.slice(0, 2) : "",
      city: typeof cf.city === "string" ? cf.city.slice(0, 100) : "",
    }) });
  }
  if (url.pathname.startsWith("/v1/admin/")) {
    if (!await authorized(request, env.ADMIN_TOKEN)) return json({ error: "unauthorized" }, 401);
    if (url.pathname === "/v1/admin/clients" && request.method === "GET") {
      const offset = url.searchParams.get("offset") || "0";
      if (!/^\d{1,7}$/.test(offset)) return json({ error: "invalid offset" }, 400);
      return store.fetch("https://access/list?offset=" + offset);
    }
    if (url.pathname === "/v1/admin/ban" && request.method === "POST") {
      let input;
      try { input = await bodyJSON(request); } catch { return json({ error: "invalid body" }, 400); }
      const ip = cleanIP(input?.ip);
      if (!ip || typeof input?.banned !== "boolean") return json({ error: "invalid ban" }, 400);
      return store.fetch("https://access/ban", { method: "POST", body: JSON.stringify({ ip, banned: input.banned }) });
    }
  }
  return json({ error: "not found" }, 404);
}

export class Access {
  constructor(state, env) {
    this.storage = state.storage;
    this.sql = state.storage.sql;
    const days = Number(env.RETENTION_DAYS || 30);
    this.days = Number.isInteger(days) && days >= 1 && days <= 365 ? days : 30;
    this.sql.exec(`CREATE TABLE IF NOT EXISTS visits (
      ip TEXT PRIMARY KEY, country TEXT NOT NULL, city TEXT NOT NULL,
      version TEXT NOT NULL, last_seen INTEGER NOT NULL);
      CREATE INDEX IF NOT EXISTS visits_seen ON visits(last_seen);
      CREATE TABLE IF NOT EXISTS bans (ip TEXT PRIMARY KEY, banned_at INTEGER NOT NULL);`);
  }
  purge(now) { this.sql.exec("DELETE FROM visits WHERE last_seen < ?", now - this.days * DAY); }
  async fetch(request) {
    const url = new URL(request.url), now = Date.now();
    if (url.pathname === "/check" && request.method === "POST") {
      const row = await request.json();
      this.sql.exec(`INSERT INTO visits(ip,country,city,version,last_seen) VALUES(?,?,?,?,?)
        ON CONFLICT(ip) DO UPDATE SET country=excluded.country,city=excluded.city,
        version=excluded.version,last_seen=excluded.last_seen`, row.ip, row.country, row.city, row.version, now);
      const banned = this.sql.exec("SELECT ip FROM bans WHERE ip = ?", row.ip).toArray().length > 0;
      if (!await this.storage.getAlarm()) await this.storage.setAlarm(now + DAY);
      return json({ allowed: !banned }, banned ? 403 : 200);
    }
    if (url.pathname === "/list" && request.method === "GET") {
      this.purge(now);
      const rows = this.sql.exec(`SELECT v.ip,v.country,v.city,v.version,v.last_seen,
        CASE WHEN b.ip IS NULL THEN 0 ELSE 1 END AS banned FROM visits v LEFT JOIN bans b ON v.ip=b.ip
        UNION ALL SELECT b.ip,'','','',0,1 FROM bans b WHERE NOT EXISTS (SELECT 1 FROM visits v WHERE v.ip=b.ip)
        ORDER BY last_seen DESC,ip LIMIT 200 OFFSET ?`, Number(url.searchParams.get("offset") || 0)).toArray();
      return json({ rows, retention_days: this.days,
        clients: this.sql.exec("SELECT COUNT(*) AS n FROM visits").one().n,
        banned: this.sql.exec("SELECT COUNT(*) AS n FROM bans").one().n,
      });
    }
    if (url.pathname === "/ban" && request.method === "POST") {
      const row = await request.json();
      if (row.banned) this.sql.exec("INSERT OR IGNORE INTO bans(ip,banned_at) VALUES(?,?)", row.ip, now);
      else this.sql.exec("DELETE FROM bans WHERE ip = ?", row.ip);
      return json({ ok: true });
    }
    return json({ error: "not found" }, 404);
  }
  async alarm() {
    this.purge(Date.now());
    if (this.sql.exec("SELECT 1 FROM visits LIMIT 1").toArray().length)
      await this.storage.setAlarm(Date.now() + DAY);
  }
}
