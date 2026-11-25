// Comentários dos recados do Qenna Writer (o mural do menu e o blog do site).
//
//   GET    /comments                → { "<id do recado>": [ { id, name, body, at }, ... ], ... }
//   POST   /comments                ← { recado, name, body, website }  (website = isca de robô, tem que vir vazio)
//                                     com Authorization: Bearer <ADMIN_KEY> = comentário do autor (selo "autor")
//   DELETE /comments/<id>           (Authorization: Bearer <ADMIN_KEY>)  · só o Pe
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

async function list(env) {
  const { results } = await env.DB.prepare(
    `SELECT id, recado, name, body, at, author FROM (
       SELECT *, ROW_NUMBER() OVER (PARTITION BY recado ORDER BY at DESC) AS n FROM comments
     ) WHERE n <= ? ORDER BY at ASC`).bind(PER_RECADO).all();
  const out = {};
  for (const c of results) (out[c.recado] ||= []).push({ id: c.id, name: c.name, body: c.body, at: c.at, author: !!c.author });
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
  const hasKey = !!env.ADMIN_KEY && (request.headers.get('Authorization') || '') === 'Bearer ' + env.ADMIN_KEY;
  const reserved = reservedName(name);
  if (!hasKey && reserved) return fail('reserved');
  const isAuthor = hasKey && reserved;
  if (!hasKey && (LINK.test(name) || LINK.test(body))) return fail('no-links');

  const now = Date.now();
  const ip = request.headers.get('CF-Connecting-IP') || 'local';
  const who = await sha256((env.SALT || 'qenna') + '|' + ip);
  await env.DB.prepare('DELETE FROM sends WHERE at < ?').bind(now - 24 * 3600 * 1000).run();
  const { n } = await env.DB.prepare('SELECT COUNT(*) AS n FROM sends WHERE who = ? AND at > ?')
    .bind(who, now - WINDOW_MS).first();
  if (!hasKey && n >= MAX_SENDS) return fail('slow-down', 429);

  const res = await env.DB.prepare('INSERT INTO comments (recado, name, body, at, author) VALUES (?, ?, ?, ?, ?)')
    .bind(recado, name, body, now, isAuthor ? 1 : 0).run();
  if (!hasKey) await env.DB.prepare('INSERT INTO sends (who, at) VALUES (?, ?)').bind(who, now).run();
  return json({ ok: true, comment: { id: res.meta.last_row_id, recado, name, body, at: now, author: isAuthor } }, 201);
}

async function remove(request, env, id) {
  const auth = request.headers.get('Authorization') || '';
  if (!env.ADMIN_KEY || auth !== 'Bearer ' + env.ADMIN_KEY) return fail('forbidden', 403);
  await env.DB.prepare('DELETE FROM comments WHERE id = ?').bind(id).run();
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
      return fail('not-found', 404);
    } catch (e) {
      return fail('server', 500);
    }
  },
};
