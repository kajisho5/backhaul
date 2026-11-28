-- One Zoom sign-in per licence, used by all its PCs. Everything from Zoom is stored
-- encrypted (AES-GCM, key in the Worker secret SIGNIN_KEY).
CREATE TABLE zoom_signins (
  licence_id INTEGER PRIMARY KEY REFERENCES licences(id) ON DELETE CASCADE,
  refresh_cipher TEXT NOT NULL,
  access_cipher TEXT,
  access_expires_at INTEGER NOT NULL DEFAULT 0,
  zak_cipher TEXT,
  zak_at INTEGER NOT NULL DEFAULT 0,
  busy_until INTEGER NOT NULL DEFAULT 0,  -- one request at a time talks to Zoom for a licence
  updated_at INTEGER NOT NULL
);
