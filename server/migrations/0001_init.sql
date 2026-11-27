-- A licence: one key, a number of PCs that may use it at the same time, an optional end.
-- The key itself is never stored, only its SHA-256.
CREATE TABLE licences (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  key_hash TEXT NOT NULL UNIQUE,
  key_tail TEXT NOT NULL,          -- last four characters, to tell keys apart in lists
  note TEXT NOT NULL DEFAULT '',
  seats INTEGER NOT NULL,
  expires_at INTEGER,              -- unix seconds; NULL = no end
  revoked INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL
);

-- A PC that was registered with a licence. `device_id` is a random number the PC made up.
CREATE TABLE devices (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  licence_id INTEGER NOT NULL REFERENCES licences(id) ON DELETE CASCADE,
  device_id TEXT NOT NULL,
  name TEXT NOT NULL,
  token_hash TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL,
  last_seen_at INTEGER NOT NULL,
  UNIQUE (licence_id, device_id)
);

-- A Zoom sign-in on its way: from "the browser was opened" to "the PC collected the result".
CREATE TABLE logins (
  state TEXT PRIMARY KEY,
  device_row INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  challenge TEXT NOT NULL,
  code TEXT,                       -- what Zoom sent back; useless without the PC's own verifier
  denied INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL
);

-- Counting requests per caller and time window, to slow down guessing.
CREATE TABLE attempts (
  bucket TEXT PRIMARY KEY,
  count INTEGER NOT NULL,
  expires_at INTEGER NOT NULL
);
