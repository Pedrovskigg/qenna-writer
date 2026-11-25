// Comentários dos recados do Qenna Writer (o mural do menu e o blog do site).
//
//   GET    /comments                → { "<id do recado>": [ { id, name, body, at }, ... ], ... }
//   POST   /comments                ← { recado, name, body, parent?, website }  (website = isca de robô, tem que vir vazio)
//                                     parent = id do comentário respondido (resposta de resposta vira resposta do original)
//   Reações nos comentários: as mesmas /reactions, com recado = "c-<id do comentário>".
//                                     com Authorization: Bearer <ADMIN_KEY> = comentário do autor (selo "autor")
//   DELETE /comments/<id>           (Authorization: Bearer <ADMIN_KEY>)  · só o Pe
//   POST   /bans/<id do comentário> (Authorization: Bearer <ADMIN_KEY>)  · bane quem escreveu e apaga o comentário
//   O POST /comments aceita "client" (o id anônimo do app/navegador), pro ban pegar o aparelho.
//
//   Bugs ("Reportar um bug" nas Configurações):
//   POST /bugs                ← { code, client, name, title, body, steps, env, website }
//   GET  /bugs/mine?client=   → os relatos de quem mandou (pelo hash do id anônimo)
//   GET  /bugs/resolved       → [{ id, title, version, updated }]  (público: só título e versão)
//   GET  /bugs/all            (chave) → todos, com nome e ambiente
//   POST /bugs/<id>           (chave) ← { stage?, reply?, version? }
//   DELETE /bugs/<id>         (chave)
//
//   GET    /reactions?me=<id>       → { "<recado>": { heart, fire, clap, laugh, mine: [kinds] }, ... }
//   POST   /reactions               ← { recado, kind, client }  (marca/desmarca; devolve o recado atualizado)
//   client = id anônimo que o app/navegador sorteia e guarda; aqui só entra o hash dele com sal.
//
// Publica na hora. As travas contra spam: nada de link, tamanho máximo, poucos
// envios por pessoa em 10 minutos e a isca de robô. A assinatura do Pe ("P.H. Lobato" em
// qualquer grafia) é reservada: só passa com a chave de admin. O IP nunca é guardado: só um
// hash dele com sal, pra contar os envios, e esse hash some depois de um dia.

const MAX_NAME = 40;
const MAX_BODY = 1000;
const MAX_SENDS = 4;            // por pessoa, a cada 10 minutos
const WINDOW_MS = 10 * 60 * 1000;
const PER_RECADO = 300;         // os mais recentes de cada recado

const cors = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, DELETE, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, Authorization',
  'Access-Control-Max-Age': '86400',
};

function json(data, status = 200, extra = {}) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { 'Content-Type': 'application/json; charset=utf-8', ...cors, ...extra },
  });
}

const fail = (code, status = 400) => json({ error: code }, status);

// "Sem links": endereço com protocolo, www., ou algo.com / algo.com.br / algo.io ...
const LINK = /(https?:\/\/|www\.|\b[a-z0-9-]+\.(com|net|org|io|br|me|ly|gg|xyz|info|app|dev|link|site|online|ru|cn|co|tk|top|shop|club)\b)/i;

// A assinatura do autor em qualquer disfarce: P.H.Lobato, PHLobato, phlobato2342a, P H L0bato,
// letras cirílicas/gregas parecidas, acentos... Tudo vira letras latinas minúsculas e compara.
const LOOKALIKE = {
  '0': 'o', '1': 'l', '!': 'l', '|': 'l', '3': 'e', '4': 'a', '@': 'a', '5': 's', '$': 's', '7': 't', '8': 'b',
  'а': 'a', 'в': 'b', 'е': 'e', 'һ': 'h', 'н': 'h', 'і': 'i', 'ӏ': 'l', 'о': 'o', 'р': 'p', 'с': 'c', 'т': 't',
  'А': 'a', 'В': 'b', 'Е': 'e', 'Н': 'h', 'І': 'i', 'О': 'o', 'Р': 'p', 'С': 'c', 'Т': 't',
  'α': 'a', 'β': 'b', 'ο': 'o', 'ρ': 'p', 'τ': 't', 'Α': 'a', 'Β': 'b', 'Η': 'h', 'Ι': 'i', 'Ο': 'o', 'Ρ': 'p', 'Τ': 't',
  'ⅼ': 'l', 'ı': 'i',
};
const RESERVED = ['phlobato', 'pedrohenriquelobato'];
function reservedName(name) {
  let s = name.normalize('NFKD').replace(/[\u0300-\u036f]/g, '');
  s = [...s].map((c) => LOOKALIKE[c] ?? c).join('').toLowerCase().replace(/[^a-z]/g, '');
  return RESERVED.some((r) => s.includes(r));
}

function clean(s, max) {
  return String(s ?? '')
    .replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F\u007F​-‏‪-‮⁦-⁩]/g, '')
    .replace(/\r\n?/g, '\n')
    .replace(/\n{3,}/g, '\n\n')
    .trim()
    .slice(0, max + 1);
}

async function sha256(text) {
  const buf = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

// A chave de admin: comparação em tempo constante e, por IP, no máximo 5 erros por hora.
// Passou disso, esse IP fica sem poder tentar a chave por 1 hora (contra chute em massa).
const MAX_KEY_FAILS = 5;
const KEY_WINDOW_MS = 60 * 60 * 1000;
function sameKey(a, b) {
  if (typeof a !== 'string' || typeof b !== 'string' || a.length !== b.length) return false;
  let d = 0;
  for (let i = 0; i < a.length; i++) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return d === 0;
}
// 'none' = não mandou chave · 'ok' = chave certa · 'bad' = errada · 'locked' = errou demais
async function checkKey(request, env) {
  const auth = request.headers.get('Authorization') || '';
  if (!auth) return 'none';
  const ip = request.headers.get('CF-Connecting-IP') || 'local';
  const who = 'k:' + await sha256((env.SALT || 'qenna') + '|' + ip);
  const now = Date.now();
  const { n } = await env.DB.prepare('SELECT COUNT(*) AS n FROM sends WHERE who = ? AND at > ?').bind(who, now - KEY_WINDOW_MS).first();
  if (n >= MAX_KEY_FAILS) return 'locked';
  if (env.ADMIN_KEY && sameKey(auth, 'Bearer ' + env.ADMIN_KEY)) return 'ok';
  await env.DB.prepare('INSERT INTO sends (who, at) VALUES (?, ?)').bind(who, now).run();
  return 'bad';
}

// Ban: o aparelho pra sempre, a conexão por 30 dias (IP de celular é dividido entre muita gente).
const IP_BAN_MS = 30 * 24 * 3600 * 1000;
async function banHashes(env, request, client) {
  const ip = request.headers.get('CF-Connecting-IP') || 'local';
  return {
    ip: await sha256((env.SALT || 'qenna') + '|ban-ip|' + ip),
    client: validClient(client) ? await sha256((env.SALT || 'qenna') + '|ban-c|' + client) : null,
  };
}
async function isBanned(env, h) {
  if (h.client && await env.DB.prepare("SELECT 1 AS x FROM bans WHERE hash = ? AND kind = 'client'").bind(h.client).first()) return true;
  return !!await env.DB.prepare("SELECT 1 AS x FROM bans WHERE hash = ? AND kind = 'ip' AND at > ?").bind(h.ip, Date.now() - IP_BAN_MS).first();
}

async function deleteThread(env, id) {
  // some junto: as respostas dele e as reações de todos
  const { results } = await env.DB.prepare('SELECT id FROM comments WHERE id = ? OR parent = ?').bind(id, id).all();
  for (const c of results) await env.DB.prepare('DELETE FROM reactions WHERE recado = ?').bind('c-' + c.id).run();
  await env.DB.prepare('DELETE FROM comments WHERE id = ? OR parent = ?').bind(id, id).run();
}

async function ban(request, env, id) {
  const key = await checkKey(request, env);
  if (key === 'locked') return fail('slow-down', 429);
  if (key !== 'ok') return fail('forbidden', 403);
  const c = await env.DB.prepare('SELECT ban_ip, ban_client FROM comments WHERE id = ?').bind(id).first();
  if (!c) return fail('gone', 404);
  const now = Date.now();
  if (c.ban_ip) await env.DB.prepare("INSERT OR REPLACE INTO bans (hash, kind, at) VALUES (?, 'ip', ?)").bind(c.ban_ip, now).run();
  if (c.ban_client) await env.DB.prepare("INSERT OR REPLACE INTO bans (hash, kind, at) VALUES (?, 'client', ?)").bind(c.ban_client, now).run();
  // some tudo o que essa pessoa escreveu (pelo aparelho; pela conexão não, que pode ser de outra gente)
  if (c.ban_client) {
    const { results } = await env.DB.prepare('SELECT id FROM comments WHERE ban_client = ?').bind(c.ban_client).all();
    for (const r of results) await deleteThread(env, r.id);
  }
  await deleteThread(env, id);
  return json({ ok: true });
}

async function list(env) {
  const { results } = await env.DB.prepare(
    `SELECT id, recado, name, body, at, author, parent FROM (
       SELECT *, ROW_NUMBER() OVER (PARTITION BY recado ORDER BY at DESC) AS n FROM comments
     ) WHERE n <= ? ORDER BY at ASC`).bind(PER_RECADO).all();
  const out = {};
  for (const c of results) (out[c.recado] ||= []).push({ id: c.id, name: c.name, body: c.body, at: c.at, author: !!c.author, parent: c.parent || null });
  return json(out, 200, { 'Cache-Control': 'no-store' });
}

async function add(request, env) {
  let data;
  try { data = await request.json(); } catch { return fail('bad-request'); }
  if (data.website) return json({ ok: true });            // robô caiu na isca: finge que deu certo
  const recado = String(data.recado ?? '');
  if (!/^[A-Za-z0-9][A-Za-z0-9_-]{1,63}$/.test(recado)) return fail('bad-request');
  const name = clean(data.name, MAX_NAME).replace(/\s+/g, ' ');
  const body = clean(data.body, MAX_BODY);
  if (!name || !body) return fail('empty');
  if (name.length > MAX_NAME || body.length > MAX_BODY) return fail('too-long');
  // A chave libera a assinatura; o selo "autor" só sai quando o nome É a assinatura
  // (o Pe comentando com outro nome, mesmo com a chave no app, sai como comentário comum).
  const key = await checkKey(request, env);
  if (key === 'locked') return fail('slow-down', 429);
  const hasKey = key === 'ok';
  const reserved = reservedName(name);
  if (!hasKey && reserved) return fail('reserved');
  const isAuthor = hasKey && reserved;
  if (!hasKey && (LINK.test(name) || LINK.test(body))) return fail('no-links');
  const bh = await banHashes(env, request, data.client);
  if (!hasKey && await isBanned(env, bh)) return fail('banned', 403);

  const now = Date.now();
  const ip = request.headers.get('CF-Connecting-IP') || 'local';
  const who = await sha256((env.SALT || 'qenna') + '|' + ip);
  await env.DB.prepare('DELETE FROM sends WHERE at < ?').bind(now - 24 * 3600 * 1000).run();
  const { n } = await env.DB.prepare('SELECT COUNT(*) AS n FROM sends WHERE who = ? AND at > ?')
    .bind(who, now - WINDOW_MS).first();
  if (!hasKey && n >= MAX_SENDS) return fail('slow-down', 429);

  // resposta: o comentário respondido tem que ser do mesmo recado; resposta de resposta sobe pro original
  let parent = null;
  if (data.parent != null && data.parent !== '') {
    const p = await env.DB.prepare('SELECT id, parent FROM comments WHERE id = ? AND recado = ?').bind(Number(data.parent), recado).first();
    if (!p) return fail('gone');
    parent = p.parent || p.id;
  }
  const res = await env.DB.prepare('INSERT INTO comments (recado, name, body, at, author, parent, ban_ip, ban_client) VALUES (?, ?, ?, ?, ?, ?, ?, ?)')
    .bind(recado, name, body, now, isAuthor ? 1 : 0, parent, hasKey ? null : bh.ip, hasKey ? null : bh.client).run();
  if (!hasKey) await env.DB.prepare('INSERT INTO sends (who, at) VALUES (?, ?)').bind(who, now).run();
  return json({ ok: true, comment: { id: res.meta.last_row_id, recado, name, body, at: now, author: isAuthor, parent } }, 201);
}

async function remove(request, env, id) {
  const key = await checkKey(request, env);
  if (key === 'locked') return fail('slow-down', 429);
  if (key !== 'ok') return fail('forbidden', 403);
  await deleteThread(env, id);
  return json({ ok: true });
}

const KINDS = ['heart', 'fire', 'clap', 'laugh'];
const MAX_TOGGLES = 60;         // marcar/desmarcar por pessoa a cada 10 minutos

const validClient = (c) => typeof c === 'string' && /^[A-Za-z0-9-]{16,64}$/.test(c);
const voterOf = (env, client) => sha256((env.SALT || 'qenna') + '|r|' + client);

async function reactionsOf(env, voter, recado) {
  const q = recado
    ? env.DB.prepare('SELECT recado, kind, COUNT(*) AS n, SUM(voter = ?) AS me FROM reactions WHERE recado = ? GROUP BY recado, kind').bind(voter || '', recado)
    : env.DB.prepare('SELECT recado, kind, COUNT(*) AS n, SUM(voter = ?) AS me FROM reactions GROUP BY recado, kind').bind(voter || '');
  const { results } = await q.all();
  const out = {};
  for (const r of results) {
    const o = (out[r.recado] ||= { heart: 0, fire: 0, clap: 0, laugh: 0, mine: [] });
    o[r.kind] = r.n;
    if (r.me) o.mine.push(r.kind);
  }
  return out;
}

async function listReactions(url, env) {
  const me = url.searchParams.get('me');
  const voter = validClient(me) ? await voterOf(env, me) : '';
  return json(await reactionsOf(env, voter), 200, { 'Cache-Control': 'no-store' });
}

async function toggleReaction(request, env) {
  let data;
  try { data = await request.json(); } catch { return fail('bad-request'); }
  const recado = String(data.recado ?? '');
  if (!/^[A-Za-z0-9][A-Za-z0-9_-]{1,63}$/.test(recado) || !KINDS.includes(data.kind) || !validClient(data.client)) return fail('bad-request');
  const now = Date.now();
  const ip = request.headers.get('CF-Connecting-IP') || 'local';
  const who = 'r:' + await sha256((env.SALT || 'qenna') + '|' + ip);
  await env.DB.prepare('DELETE FROM sends WHERE at < ?').bind(now - 24 * 3600 * 1000).run();
  const { n } = await env.DB.prepare('SELECT COUNT(*) AS n FROM sends WHERE who = ? AND at > ?').bind(who, now - WINDOW_MS).first();
  if (n >= MAX_TOGGLES) return fail('slow-down', 429);
  if (await isBanned(env, await banHashes(env, request, data.client))) return fail('banned', 403);
  const voter = await voterOf(env, data.client);
  const del = await env.DB.prepare('DELETE FROM reactions WHERE recado = ? AND kind = ? AND voter = ?').bind(recado, data.kind, voter).run();
  if (!del.meta.changes) await env.DB.prepare('INSERT INTO reactions (recado, kind, voter, at) VALUES (?, ?, ?, ?)').bind(recado, data.kind, voter, now).run();
  await env.DB.prepare('INSERT INTO sends (who, at) VALUES (?, ?)').bind(who, now).run();
  const all = await reactionsOf(env, voter, recado);
  return json({ ok: true, recado, reactions: all[recado] || { heart: 0, fire: 0, clap: 0, laugh: 0, mine: [] } });
}

const BUG_CODE = /^QW-[23456789ABCDEFGHJKMNPQRSTUVWXYZ]{5}$/;
const BUG_STAGES = ['sent', 'work', 'done'];
const MAX_BUGS = 6;                       // relatos por pessoa por hora
const bugClient = (env, client) => sha256((env.SALT || 'qenna') + '|bug|' + client);
function bugOut(b, withName) {
  const o = { id: b.id, code: b.code, title: b.title, body: b.body, steps: b.steps, env: b.env, stage: b.stage,
              reply: b.reply, reply_at: b.reply_at, version: b.version, at: b.at, updated: b.updated };
  if (withName) o.name = b.name;
  return o;
}

async function addBug(request, env) {
  let d;
  try { d = await request.json(); } catch { return fail('bad-request'); }
  if (d.website) return json({ ok: true });
  if (!BUG_CODE.test(String(d.code || '')) || !validClient(d.client)) return fail('bad-request');
  const name = clean(d.name, MAX_NAME).replace(/\s+/g, ' ') || 'Anônimo';
  const title = clean(d.title, 80).replace(/\s+/g, ' ');
  const body = clean(d.body, 2000);
  const steps = clean(d.steps, 1000);
  const envTxt = clean(d.env, 160).replace(/\s+/g, ' ');
  if (!title || !body) return fail('empty');
  if (title.length > 80 || body.length > 2000 || steps.length > 1000 || name.length > MAX_NAME) return fail('too-long');
  const bh = await banHashes(env, request, d.client);
  if (await isBanned(env, bh)) return fail('banned', 403);
  const now = Date.now();
  const ip = request.headers.get('CF-Connecting-IP') || 'local';
  const who = 'b:' + await sha256((env.SALT || 'qenna') + '|' + ip);
  await env.DB.prepare('DELETE FROM sends WHERE at < ?').bind(now - 24 * 3600 * 1000).run();
  const { n } = await env.DB.prepare('SELECT COUNT(*) AS n FROM sends WHERE who = ? AND at > ?').bind(who, now - 3600 * 1000).first();
  if (n >= MAX_BUGS) return fail('slow-down', 429);
  if (await env.DB.prepare('SELECT 1 AS x FROM bugs WHERE code = ?').bind(d.code).first()) return fail('code-taken', 409);
  const res = await env.DB.prepare('INSERT INTO bugs (code, client, ban_ip, name, title, body, steps, env, at, updated) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)')
    .bind(d.code, await bugClient(env, d.client), bh.ip, name, title, body, steps, envTxt, now, now).run();
  await env.DB.prepare('INSERT INTO sends (who, at) VALUES (?, ?)').bind(who, now).run();
  const b = await env.DB.prepare('SELECT * FROM bugs WHERE id = ?').bind(res.meta.last_row_id).first();
  return json({ ok: true, bug: bugOut(b, false) }, 201);
}

async function myBugs(url, env) {
  const c = url.searchParams.get('client');
  if (!validClient(c)) return fail('bad-request');
  const { results } = await env.DB.prepare('SELECT * FROM bugs WHERE client = ? ORDER BY at DESC LIMIT 200').bind(await bugClient(env, c)).all();
  return json(results.map((b) => bugOut(b, false)), 200, { 'Cache-Control': 'no-store' });
}

async function resolvedBugs(env) {
  const { results } = await env.DB.prepare("SELECT id, title, version, updated FROM bugs WHERE stage = 'done' AND version != '' ORDER BY updated DESC LIMIT 500").all();
  return json(results, 200, { 'Cache-Control': 'no-store' });
}

async function allBugs(request, env) {
  const key = await checkKey(request, env);
  if (key === 'locked') return fail('slow-down', 429);
  if (key !== 'ok') return fail('forbidden', 403);
  const { results } = await env.DB.prepare('SELECT * FROM bugs ORDER BY at DESC LIMIT 1000').all();
  return json(results.map((b) => bugOut(b, true)), 200, { 'Cache-Control': 'no-store' });
}

async function updateBug(request, env, id) {
  const key = await checkKey(request, env);
  if (key === 'locked') return fail('slow-down', 429);
  if (key !== 'ok') return fail('forbidden', 403);
  let d;
  try { d = await request.json(); } catch { return fail('bad-request'); }
  const b = await env.DB.prepare('SELECT * FROM bugs WHERE id = ?').bind(id).first();
  if (!b) return fail('gone', 404);
  const now = Date.now();
  const stage = BUG_STAGES.includes(d.stage) ? d.stage : b.stage;
  const reply = d.reply != null ? clean(d.reply, 2000) : b.reply;
  const version = d.version != null ? clean(d.version, 20) : b.version;
  const replyAt = reply !== b.reply ? (reply ? now : null) : b.reply_at;
  await env.DB.prepare('UPDATE bugs SET stage = ?, reply = ?, reply_at = ?, version = ?, updated = ? WHERE id = ?')
    .bind(stage, reply, replyAt, version, now, id).run();
  const nb = await env.DB.prepare('SELECT * FROM bugs WHERE id = ?').bind(id).first();
  return json({ ok: true, bug: bugOut(nb, true) });
}

async function removeBug(request, env, id) {
  const key = await checkKey(request, env);
  if (key === 'locked') return fail('slow-down', 429);
  if (key !== 'ok') return fail('forbidden', 403);
  await env.DB.prepare('DELETE FROM bugs WHERE id = ?').bind(id).run();
  return json({ ok: true });
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: cors });
    try {
      if (url.pathname === '/comments' && request.method === 'GET') return await list(env);
      if (url.pathname === '/comments' && request.method === 'POST') return await add(request, env);
      const m = url.pathname.match(/^\/comments\/(\d+)$/);
      if (m && request.method === 'DELETE') return await remove(request, env, Number(m[1]));
      const mb = url.pathname.match(/^\/bans\/(\d+)$/);
      if (mb && request.method === 'POST') return await ban(request, env, Number(mb[1]));
      if (url.pathname === '/reactions' && request.method === 'GET') return await listReactions(url, env);
      if (url.pathname === '/bugs' && request.method === 'POST') return await addBug(request, env);
      if (url.pathname === '/bugs/mine' && request.method === 'GET') return await myBugs(url, env);
      if (url.pathname === '/bugs/resolved' && request.method === 'GET') return await resolvedBugs(env);
      if (url.pathname === '/bugs/all' && request.method === 'GET') return await allBugs(request, env);
      const mbug = url.pathname.match(/^\/bugs\/(\d+)$/);
      if (mbug && request.method === 'POST') return await updateBug(request, env, Number(mbug[1]));
      if (mbug && request.method === 'DELETE') return await removeBug(request, env, Number(mbug[1]));
      if (url.pathname === '/reactions' && request.method === 'POST') return await toggleReaction(request, env);
      return fail('not-found', 404);
    } catch (e) {
      return fail('server', 500);
    }
  },
};
