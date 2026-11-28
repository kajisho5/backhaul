-- Whether a subscription is set to end with the period already paid (the buyer cancelled,
-- and it runs out). The program uses this to tell "ends soon" from "is simply renewed soon":
-- a licence that renews by itself must not be reported as about to end.
ALTER TABLE subscriptions ADD COLUMN ending INTEGER NOT NULL DEFAULT 0;
