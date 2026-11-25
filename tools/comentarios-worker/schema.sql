CREATE TABLE IF NOT EXISTS comments (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  recado  TEXT NOT NULL,
  name    TEXT NOT NULL,
  body    TEXT NOT NULL,
  at      INTEGER NOT NULL,
  author  INTEGER NOT NULL DEFAULT 0,  -- 1 = o Pe (enviou com a chave de admin)
  parent  INTEGER,                     -- resposta a este comentário (um nível só)
  ban_ip     TEXT,                     -- hashes (com sal) pra poder banir quem escreveu; nunca saem do servidor
  ban_client TEXT
);
-- banidos: o aparelho (id anônimo do app/navegador) pra sempre; a conexão (IP) por 30 dias,
-- porque operadora de celular divide o mesmo IP entre muita gente
-- "Reportar um bug" (Configurações do app). Quem relatou só vê os seus (pelo hash do id anônimo);
-- o histórico público mostra só título e versão dos resolvidos; o Pe vê e mexe em tudo com a chave.
CREATE TABLE IF NOT EXISTS bugs (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  code     TEXT NOT NULL UNIQUE,    -- protocolo QW-XXXXX (vai no assunto do e-mail com o print)
  client   TEXT NOT NULL,           -- hash com sal do id anônimo de quem relatou
  ban_ip   TEXT,
  name     TEXT NOT NULL,
  title    TEXT NOT NULL,
  body     TEXT NOT NULL,
  steps    TEXT NOT NULL DEFAULT '',
  env      TEXT NOT NULL DEFAULT '',
  stage    TEXT NOT NULL DEFAULT 'sent',   -- sent | work | done
  reply    TEXT NOT NULL DEFAULT '',
  reply_at INTEGER,
  version  TEXT NOT NULL DEFAULT '',
  at       INTEGER NOT NULL,
  updated  INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS bugs_client ON bugs (client);
CREATE TABLE IF NOT EXISTS bans (
  hash  TEXT PRIMARY KEY,
  kind  TEXT NOT NULL,      -- 'ip' | 'client'
  at    INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS comments_recado ON comments (recado, at);
-- limite de envios: só o hash do IP com sal (nunca o IP), apagado depois de um dia
CREATE TABLE IF NOT EXISTS sends (
  who  TEXT NOT NULL,
  at   INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS sends_who ON sends (who, at);
-- reações: uma de cada tipo por pessoa e recado. "voter" = hash (com sal) do id
-- anônimo que o app/navegador sorteia; nada que identifique a pessoa.
CREATE TABLE IF NOT EXISTS reactions (
  recado  TEXT NOT NULL,
  kind    TEXT NOT NULL,
  voter   TEXT NOT NULL,
  at      INTEGER NOT NULL,
  PRIMARY KEY (recado, kind, voter)
);
