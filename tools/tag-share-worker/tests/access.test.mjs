import test from "node:test";
import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";
import worker, { Room } from "../src/index.js";
import { Access, cleanIP } from "../src/access.js";

function setup({ legacy = false } = {}) {
  const db = new DatabaseSync(":memory:");
  if (legacy) db.exec(`CREATE TABLE visits(ip TEXT PRIMARY KEY,country TEXT NOT NULL,city TEXT NOT NULL,version TEXT NOT NULL,last_seen INTEGER NOT NULL);
    CREATE TABLE bans(ip TEXT PRIMARY KEY,banned_at INTEGER NOT NULL);
    INSERT INTO visits VALUES('203.0.113.10','KR','Seoul','old',${Date.now()});
    INSERT INTO bans VALUES('203.0.113.10',${Date.now()});`);
  let alarm = null;
  const storage = {
    sql: { exec(sql, ...params) {
      if (sql.includes("CREATE TABLE")) { db.exec(sql); return; }
      const rows = db.prepare(sql).all(...params);
      return { toArray: () => rows, one: () => { assert.equal(rows.length, 1); return rows[0]; } };
    } },
    getAlarm: async () => alarm, setAlarm: async value => { alarm = value; },
  };
  const env = { ADMIN_TOKEN: "test-only-administrator-secret-123456789", RETENTION_DAYS: "30" };
  const access = new Access({ storage }, env);
  env.ACCESS = { idFromName: name => name, get: () => ({ fetch: (url, init) => access.fetch(new Request(url, init)) }) };
  const call = async (path, body, { admin = false, ip = "203.0.113.10", token = env.ADMIN_TOKEN,
    cf = { country: "KR", city: "Seoul" } } = {}) => {
    const headers = { "CF-Connecting-IP": ip };
    if (admin) headers.authorization = "Bearer " + token;
    const request = new Request("https://relay.example" + path, {
      method: body === undefined ? "GET" : "POST", headers,
      body: body === undefined ? undefined : JSON.stringify(body),
    });
    Object.defineProperty(request, "cf", { value: cf });
    return worker.fetch(request, env);
  };
  return { db, env, access, call, storage };
}

test("IP canonicalization rejects hostnames, ports, zones and malformed values", () => {
  assert.equal(cleanIP("203.0.113.10"), "203.0.113.10");
  assert.equal(cleanIP("2001:0DB8:0:0:0:0:0:1"), "2001:db8::1");
  for (const ip of ["example.com", "256.1.1.1", "01.2.3.4", "1.2.3.4:80", "::1%eth0", "[::1]", ":::1", "", null])
    assert.equal(cleanIP(ip), null, String(ip));
});

test("deployment probe verifies the storage implementation without recording a visit", async () => {
  const { db, call, env } = setup();
  try {
    const response = await call("/v1/access");
    assert.equal(response.status, 200);
    assert.equal(response.headers.get("cache-control"), "no-store");
    assert.deepEqual(await response.json(), { schema: 2 });
    assert.equal(db.prepare("SELECT COUNT(*) AS n FROM visits").get().n, 0);
    assert.equal(db.prepare("SELECT COUNT(*) AS n FROM bans").get().n, 0);
    env.ADMIN_TOKEN = "";
    assert.equal((await call("/v1/access")).status, 503);
  } finally { db.close(); }
});

test("authenticated administrator can list, ban and unblock; trusted edge metadata wins", async () => {
  const { db, call, storage } = setup();
  try {
    assert.equal((await call("/v1/admin/clients")).status, 401);
    assert.equal((await call("/v1/admin/ban", { ip: "203.0.113.10", banned: true }, { admin: true, token: "wrong" })).status, 401);
    assert.equal((await call("/v1/access", { version: "2.6-ntl0.101", ip: "1.2.3.4", city: "fake", nickname: "not retained" })).status, 200);
    let data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.equal(data.rows.length, 1);
    assert.deepEqual(Object.keys(data.rows[0]).sort(), ["banned", "city", "country", "ip", "last_seen", "location", "version"]);
    assert.equal(data.rows[0].ip, "203.0.113.10");
    assert.equal(data.rows[0].city, "Seoul"); assert.equal(data.rows[0].country, "KR");
    assert.ok(data.rows[0].last_seen > 0); assert.ok(await storage.getAlarm());
    assert.equal((await call("/v1/admin/ban", { ip: "203.0.113.10", banned: true }, { admin: true })).status, 200);
    let response = await call("/v1/access", { version: "2.6-ntl0.101" });
    assert.equal(response.status, 403); assert.deepEqual(await response.json(), { allowed: false });
    assert.equal((await call("/v1/admin/ban", { ip: "203.0.113.10", banned: false }, { admin: true })).status, 200);
    response = await call("/v1/access", { version: "2.6-ntl0.101" });
    assert.equal(response.status, 200); assert.deepEqual(await response.json(), { allowed: true });
    data = await (await call("/v1/admin/clients", undefined, { admin: true })).json(); assert.equal(data.banned, 0);
  } finally { db.close(); }
});

test("IPv6 bans use canonical addresses; alarms remove old visits and retain explicit bans", async () => {
  const { db, call, access, storage } = setup();
  try {
    await call("/v1/admin/ban", { ip: "2001:0db8:0:0:0:0:0:1", banned: true }, { admin: true });
    assert.equal((await call("/v1/access", { version: "test" }, { ip: "2001:db8::1" })).status, 403);
    db.prepare("UPDATE visits SET last_seen = ?").run(Date.now() - 31 * 86400000);
    await access.alarm();
    const data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.equal(data.clients, 0); assert.equal(data.banned, 1);
    assert.equal(data.rows[0].last_seen, 0); assert.equal(data.rows[0].ip, "2001:db8::1");
    // Restarting the object preserves bans because they live in SQLite.
    const restarted = new Access({ storage }, { RETENTION_DAYS: "30" });
    assert.equal((await restarted.fetch(new Request("https://access/check", { method: "POST", body: JSON.stringify({ ip: "2001:db8::1", version: "test", country: "", city: "" }) }))).status, 403);
  } finally { db.close(); }
});

test("invalid requests never write records; administrator page reveals no secret", async () => {
  const { db, env, call } = setup();
  try {
    for (const input of [null, {}, { version: "x".repeat(49) }, { version: "<script>" }, { version: "ok", padding: "x".repeat(513) }])
      assert.equal((await call("/v1/access", input)).status, 400);
    assert.equal((await call("/v1/access", { version: "test" }, { ip: "fake" })).status, 503);
    for (const input of [{ ip: "host.example", banned: true }, { ip: "1.2.3.4", banned: "yes" }, null])
      assert.equal((await call("/v1/admin/ban", input, { admin: true })).status, 400);
    assert.equal((await call("/v1/admin/clients?offset=garbage", undefined, { admin: true })).status, 400);
    const page = await call("/admin");
    assert.equal(page.headers.get("cache-control"), "no-store");
    assert.ok(page.headers.get("content-security-policy").includes("frame-ancestors 'none'"));
    assert.ok(!(await page.text()).includes(env.ADMIN_TOKEN));
    env.ADMIN_TOKEN = "";
    assert.equal((await call("/v1/access", { version: "test" })).status, 503);
    assert.equal(db.prepare("SELECT COUNT(*) AS n FROM visits").get().n, 0);
  } finally { db.close(); }
});

test("existing tag sharing remains independent of access control", async () => {
  const room = new Room({});
  const result = await room.fetch(new Request("https://room/", { method: "POST", body: JSON.stringify({ sid: 123, tag: ["ntl", 8] }) }));
  assert.deepEqual(await result.json(), { p: [[123, "ntl", 8]] });
});

test("detailed location comes only from edge metadata and refreshes per IP", async () => {
  const { db, call } = setup();
  try {
    const cf = { country: "KR", city: "Seoul", region: "Seoul", regionCode: "11", postalCode: "04524",
      latitude: "37.56650", longitude: "126.97800", timezone: "Asia/Seoul", asOrganization: "Example Network" };
    await call("/v1/access", { version: "test", location: { region: "fake", latitude: 12 }, postalCode: "fake" }, { cf });
    let data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.deepEqual(data.rows[0].location, { region: "Seoul", regionCode: "11", postalCode: "04524",
      latitude: 37.5665, longitude: 126.978, timezone: "Asia/Seoul", asOrganization: "Example Network" });
    await call("/v1/access", { version: "new" }, { cf: { latitude: "0", longitude: 0 } });
    data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.equal(data.rows.length, 1); assert.equal(data.rows[0].version, "new");
    assert.equal(data.rows[0].location.latitude, 0); assert.equal(data.rows[0].location.longitude, 0);
    assert.equal(data.rows[0].location.region, ""); assert.equal(data.rows[0].location.postalCode, "");
  } finally { db.close(); }
});

test("absent or invalid location fields remain unknown; strings are bounded", async () => {
  const { db, call } = setup();
  try {
    for (const cf of [undefined, null, {}, { latitude: "", longitude: " " }, { latitude: null, longitude: null },
      { latitude: "91", longitude: "-181" }, { latitude: "NaN", longitude: "Infinity" },
      { latitude: true, longitude: [] }]) {
      await call("/v1/access", { version: "test" }, { cf });
      const data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
      assert.equal(data.rows[0].location.latitude, null); assert.equal(data.rows[0].location.longitude, null);
    }
    await call("/v1/access", { version: "test" }, { cf: { region: "a".repeat(1000), postalCode: 12345, asOrganization: "a".repeat(1000) } });
    const data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.equal(data.rows[0].location.region.length, 100);
    assert.equal(data.rows[0].location.asOrganization.length, 200);
    assert.equal(data.rows[0].location.postalCode, "");
  } finally { db.close(); }
});

test("migration preserves legacy visits and bans and is safe on restart", async () => {
  const { db, call, storage } = setup({ legacy: true });
  try {
    let data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.equal(data.rows[0].version, "old"); assert.equal(data.rows[0].city, "Seoul");
    assert.equal(data.rows[0].banned, 1); assert.equal(data.rows[0].location.latitude, null);
    new Access({ storage }, {});
    assert.equal((await call("/v1/access", { version: "new" }, { cf: { region: "Seoul", latitude: "37", longitude: "127" } })).status, 403);
    data = await (await call("/v1/admin/clients", undefined, { admin: true })).json();
    assert.equal(data.rows[0].location.latitude, 37); assert.equal(data.rows[0].banned, 1);
  } finally { db.close(); }
});
