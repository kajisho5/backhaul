-- A Stripe subscription and the licence it pays for. The licence key is kept here, sealed
-- (AES-GCM, like the Zoom sign-ins), only until the buyer has it: it is removed once it was
-- both shown on the "thank you" page and sent by e-mail, or a week after the e-mail.
-- No e-mail address is stored: Stripe has it, and it is read from there when a mail is sent.
CREATE TABLE subscriptions (
  stripe_id TEXT PRIMARY KEY,           -- sub_…
  licence_id INTEGER NOT NULL UNIQUE REFERENCES licences(id) ON DELETE CASCADE,
  status TEXT NOT NULL,                 -- Stripe's word: active, past_due, canceled, …
  key_cipher TEXT,
  mailed_at INTEGER,                    -- when the key was e-mailed (set while sending, cleared if that failed)
  shown_at INTEGER,                     -- when the key was shown on the page
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);

-- One free trial per e-mail address. Only a hash of the address is kept.
CREATE TABLE trials (
  email_hash TEXT PRIMARY KEY,
  licence_id INTEGER NOT NULL REFERENCES licences(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL
);
