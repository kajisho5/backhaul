// Runs the service's real SQL against a real SQLite (the engine behind D1), in memory.
// Only the small part of the D1 interface the service uses is provided.
import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { DatabaseSync } from "node:sqlite";

type Value = string | number | null;

class Statement {
  private values: Value[] = [];
  constructor(
    private readonly db: DatabaseSync,
    private readonly sql: string,
  ) {}
  bind(...values: Value[]) {
    const next = new Statement(this.db, this.sql);
    next.values = values;
    return next;
  }
  async first<T>(): Promise<T | null> {
    return (this.db.prepare(this.sql).get(...this.values) as T | undefined) ?? null;
  }
  async all<T>(): Promise<{ results: T[] }> {
    return { results: this.db.prepare(this.sql).all(...this.values) as T[] };
  }
  async run(): Promise<{ meta: { changes: number } }> {
    const done = this.db.prepare(this.sql).run(...this.values);
    return { meta: { changes: Number(done.changes) } };
  }
}

export function memoryD1(): D1Database {
  const db = new DatabaseSync(":memory:");
  db.exec("PRAGMA foreign_keys = ON");
  const folder = join(import.meta.dirname, "..", "migrations");
  for (const file of readdirSync(folder).sort()) db.exec(readFileSync(join(folder, file), "utf8"));
  return { prepare: (sql: string) => new Statement(db, sql) } as unknown as D1Database;
}
