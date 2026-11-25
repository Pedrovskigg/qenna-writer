CREATE TABLE IF NOT EXISTS comments (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  recado  TEXT NOT NULL,
  name    TEXT NOT NULL,
  body    TEXT NOT NULL,
  at      INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS comments_recado ON comments (recado, at);
-- limite de envios: só o hash do IP com sal (nunca o IP), apagado depois de um dia
CREATE TABLE IF NOT EXISTS sends (
  who  TEXT NOT NULL,
  at   INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS sends_who ON sends (who, at);
