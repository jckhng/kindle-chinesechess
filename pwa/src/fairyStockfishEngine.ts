import type { Difficulty, Move } from "./types";

type EngineState = "loading" | "ready" | "fallback";

const ENGINE_WORKER_URL = `${import.meta.env.BASE_URL}engine/fairy-stockfish/engine-worker.js`;
const MOVE_TIMEOUT_MS = 3500;
const files = "abcdefghi";

export function moveToUci(move: Move): string {
  return `${files[move.fromCol]}${10 - move.fromRow}${files[move.toCol]}${10 - move.toRow}`;
}

export function parseUciMove(token: string): { fromRow: number; fromCol: number; toRow: number; toCol: number } | null {
  const match = /^([a-i])(10|[1-9])([a-i])(10|[1-9])/.exec(token);
  if (!match) return null;
  return {
    fromCol: files.indexOf(match[1]),
    fromRow: 10 - Number(match[2]),
    toCol: files.indexOf(match[3]),
    toRow: 10 - Number(match[4])
  };
}

export class FairyStockfishEngine {
  private worker: Worker | null = null;
  private pending: ((move: string | null) => void) | null = null;
  private readyWaiters: Array<(ready: boolean) => void> = [];
  private moveTimer = 0;
  private state: EngineState = "loading";

  constructor(private readonly onState: (state: EngineState, detail?: string) => void) {}

  init(): void {
    if (this.worker) return;
    if (typeof Worker === "undefined" || typeof SharedArrayBuffer === "undefined") {
      this.setState("fallback", "SharedArrayBuffer unavailable.");
      return;
    }
    try {
      this.worker = new Worker(ENGINE_WORKER_URL);
      this.worker.onmessage = (event) => this.handleMessage(event.data);
      this.worker.onerror = (event) => this.setState("fallback", event.message || "Fairy-Stockfish worker failed.");
      this.worker.postMessage({ type: "init" });
    } catch (error) {
      this.setState("fallback", error instanceof Error ? error.message : String(error));
    }
  }

  isReady(): boolean {
    return this.state === "ready" && Boolean(this.worker);
  }

  newGame(): void {
    if (this.isReady()) this.worker?.postMessage({ type: "newgame" });
  }

  async requestMove(history: Move[], level: Difficulty): Promise<string | null> {
    if (!this.worker || this.state === "fallback") return null;
    if (this.state === "loading") {
      const ready = await this.waitUntilReady(2500);
      if (!ready || !this.worker) return null;
    }
    if (this.pending) this.pending(null);
    return new Promise((resolve) => {
      this.pending = resolve;
      window.clearTimeout(this.moveTimer);
      this.moveTimer = window.setTimeout(() => this.resolveMove(null), MOVE_TIMEOUT_MS);
      this.worker?.postMessage({ type: "go", moves: history.map(moveToUci), level });
    });
  }

  dispose(): void {
    window.clearTimeout(this.moveTimer);
    this.pending?.(null);
    this.pending = null;
    this.worker?.terminate();
    this.worker = null;
  }

  private handleMessage(message: { type?: string; move?: string; reason?: string }): void {
    if (message.type === "ready") {
      this.setState("ready");
      this.resolveReadyWaiters(true);
      return;
    }
    if (message.type === "unavailable") {
      if (message.reason === "Engine is not ready.") return;
      this.setState("fallback", message.reason);
      this.resolveReadyWaiters(false);
      this.resolveMove(null);
      return;
    }
    if (message.type === "bestmove") {
      this.resolveMove(message.move && message.move !== "(none)" ? message.move : null);
    }
  }

  private resolveMove(move: string | null): void {
    window.clearTimeout(this.moveTimer);
    const resolve = this.pending;
    this.pending = null;
    resolve?.(move);
  }

  private setState(state: EngineState, detail?: string): void {
    this.state = state;
    this.onState(state, detail);
  }

  private waitUntilReady(timeoutMs: number): Promise<boolean> {
    if (this.state === "ready") return Promise.resolve(true);
    if (this.state === "fallback") return Promise.resolve(false);
    return new Promise((resolve) => {
      const timer = window.setTimeout(() => {
        this.readyWaiters = this.readyWaiters.filter((waiter) => waiter !== done);
        resolve(false);
      }, timeoutMs);
      const done = (ready: boolean) => {
        window.clearTimeout(timer);
        resolve(ready);
      };
      this.readyWaiters.push(done);
    });
  }

  private resolveReadyWaiters(ready: boolean): void {
    const waiters = this.readyWaiters;
    this.readyWaiters = [];
    waiters.forEach((waiter) => waiter(ready));
  }
}
