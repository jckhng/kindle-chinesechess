import { useEffect, useMemo, useRef, useState } from "react";
import { FairyStockfishEngine, parseUciMove } from "./fairyStockfishEngine";
import type { Difficulty, Game, Mode, Move, Piece, PieceType, Side } from "./types";

type PersistedState = {
  game: Game;
  savedGame: Game | null;
  mode: Mode;
  difficulty: Difficulty;
  flipped: boolean;
};

const STORAGE_KEY = "exact-chinesechess-pwa-state-v1";
const ROWS = 10;
const COLS = 9;
const MAX_MOVES = 256;
const PUBLIC_BASE = import.meta.env.BASE_URL;
const pieceValues: Record<PieceType, number> = {
  general: 10000, advisor: 20, elephant: 40, horse: 60, rook: 100, cannon: 80, soldier: 10
};
const pieceText: Record<Side, Record<PieceType, string>> = {
  red: { general: "帥", advisor: "仕", elephant: "相", horse: "馬", rook: "車", cannon: "炮", soldier: "兵" },
  black: { general: "將", advisor: "士", elephant: "象", horse: "馬", rook: "車", cannon: "砲", soldier: "卒" }
};
const pieceName: Record<PieceType, string> = {
  general: "General", advisor: "Advisor", elephant: "Elephant",
  horse: "Horse", rook: "Rook", cannon: "Cannon", soldier: "Soldier"
};
const pieceAssetName: Record<PieceType, string> = {
  general: "k", advisor: "a", elephant: "b", horse: "n", rook: "r", cannon: "c", soldier: "p"
};
const blackStart: Array<[number, number, PieceType]> = [
  [0, 0, "rook"], [0, 1, "horse"], [0, 2, "elephant"], [0, 3, "advisor"], [0, 4, "general"],
  [0, 5, "advisor"], [0, 6, "elephant"], [0, 7, "horse"], [0, 8, "rook"],
  [2, 1, "cannon"], [2, 7, "cannon"],
  [3, 0, "soldier"], [3, 2, "soldier"], [3, 4, "soldier"], [3, 6, "soldier"], [3, 8, "soldier"]
];

function otherSide(side: Side): Side { return side === "red" ? "black" : "red"; }
function inBounds(row: number, col: number): boolean { return row >= 0 && row < ROWS && col >= 0 && col < COLS; }

function createGame(): Game {
  const pieces: Piece[] = [];
  blackStart.forEach(([row, col, type], index) => {
    pieces.push({ id: index, row, col, type, side: "black", dead: false });
    pieces.push({ id: index + 16, row: 9 - row, col: 8 - col, type, side: "red", dead: false });
  });
  pieces.sort((a, b) => a.id - b.id);
  return { pieces, history: [], turn: "red", gameOver: false, winner: "black" };
}

function cloneGame(game: Game): Game {
  return {
    pieces: game.pieces.map((p) => ({ ...p })),
    history: game.history.map((m) => ({ ...m })),
    turn: game.turn, gameOver: game.gameOver, winner: game.winner
  };
}

function normalizeGame(value: unknown): Game | null {
  if (!value || typeof value !== "object") return null;
  const game = value as Game;
  if (!Array.isArray(game.pieces) || !Array.isArray(game.history) || (game.turn !== "red" && game.turn !== "black")) return null;
  if (game.pieces.length !== 32) return null;
  return cloneGame(game);
}

function loadState(): PersistedState {
  const fallback: PersistedState = { game: createGame(), savedGame: null, mode: "red", difficulty: "medium", flipped: false };
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return fallback;
    const parsed = JSON.parse(raw) as Partial<PersistedState>;
    return {
      game: normalizeGame(parsed.game) || fallback.game,
      savedGame: normalizeGame(parsed.savedGame) || null,
      mode: parsed.mode || fallback.mode,
      difficulty: parsed.difficulty || fallback.difficulty,
      flipped: Boolean(parsed.flipped)
    };
  } catch { return fallback; }
}

function pieceAt(game: Game, row: number, col: number): number {
  if (!inBounds(row, col)) return -1;
  return game.pieces.findIndex((p) => !p.dead && p.row === row && p.col === col);
}

function lineCount(game: Game, row1: number, col1: number, row2: number, col2: number): number {
  let count = 0;
  if (row1 !== row2 && col1 !== col2) return -1;
  if (row1 === row2) {
    const min = Math.min(col1, col2), max = Math.max(col1, col2);
    for (let col = min + 1; col < max; col++) if (pieceAt(game, row1, col) !== -1) count++;
  } else {
    const min = Math.min(row1, row2), max = Math.max(row1, row2);
    for (let row = min + 1; row < max; row++) if (pieceAt(game, row, col1) !== -1) count++;
  }
  return count;
}

function palaceContains(side: Side, row: number, col: number): boolean {
  if (col < 3 || col > 5) return false;
  return side === "red" ? row >= 7 && row <= 9 : row >= 0 && row <= 2;
}

function rawMoveLegal(game: Game, moveId: number, captureId: number, row: number, col: number): boolean {
  if (moveId < 0 || moveId >= game.pieces.length || !inBounds(row, col)) return false;
  const piece = game.pieces[moveId];
  if (piece.dead) return false;
  if (captureId !== -1 && game.pieces[captureId].side === piece.side) return false;
  const dr = row - piece.row, dc = col - piece.col;
  const adr = Math.abs(dr), adc = Math.abs(dc);
  switch (piece.type) {
    case "general":
      if (captureId !== -1 && game.pieces[captureId].type === "general") return lineCount(game, piece.row, piece.col, row, col) === 0;
      return palaceContains(piece.side, row, col) && ((adr === 1 && adc === 0) || (adr === 0 && adc === 1));
    case "advisor": return palaceContains(piece.side, row, col) && adr === 1 && adc === 1;
    case "elephant":
      if (adr !== 2 || adc !== 2) return false;
      if (pieceAt(game, (piece.row + row) / 2, (piece.col + col) / 2) !== -1) return false;
      return piece.side === "red" ? row >= 5 : row <= 4;
    case "horse":
      if (!((adr === 1 && adc === 2) || (adr === 2 && adc === 1))) return false;
      return adr === 1 ? pieceAt(game, piece.row, (piece.col + col) / 2) === -1 : pieceAt(game, (piece.row + row) / 2, piece.col) === -1;
    case "rook": return lineCount(game, piece.row, piece.col, row, col) === 0;
    case "cannon": { const c = lineCount(game, piece.row, piece.col, row, col); return captureId === -1 ? c === 0 : c === 1; }
    case "soldier":
      if (!((adr === 1 && adc === 0) || (adr === 0 && adc === 1))) return false;
      if (piece.side === "red") { if (dr > 0) return false; if (piece.row >= 5 && dr === 0) return false; }
      else { if (dr < 0) return false; if (piece.row <= 4 && dr === 0) return false; }
      return true;
  }
}

function fakeMove(game: Game, move: Move): void {
  if (move.captureId !== -1) game.pieces[move.captureId].dead = true;
  game.pieces[move.moveId].row = move.toRow;
  game.pieces[move.moveId].col = move.toCol;
  game.turn = otherSide(game.turn);
}

function unfakeMove(game: Game, move: Move): void {
  game.turn = otherSide(game.turn);
  game.pieces[move.moveId].row = move.fromRow;
  game.pieces[move.moveId].col = move.fromCol;
  if (move.captureId !== -1) game.pieces[move.captureId].dead = false;
}

function inCheck(game: Game, side: Side): boolean {
  const general = side === "red" ? 20 : 4;
  if (game.pieces[general].dead) return true;
  for (const piece of game.pieces) {
    if (piece.dead || piece.side === side) continue;
    if (rawMoveLegal(game, piece.id, general, game.pieces[general].row, game.pieces[general].col)) return true;
  }
  return false;
}

function isLegalMove(game: Game, moveId: number, row: number, col: number): boolean {
  if (moveId < 0 || moveId >= game.pieces.length || game.pieces[moveId].side !== game.turn) return false;
  const captureId = pieceAt(game, row, col);
  if (!rawMoveLegal(game, moveId, captureId, row, col)) return false;
  const copy = cloneGame(game);
  fakeMove(copy, { moveId, captureId, fromRow: game.pieces[moveId].row, fromCol: game.pieces[moveId].col, toRow: row, toCol: col });
  return !inCheck(copy, game.turn);
}

function generateMoves(game: Game, side: Side, maxMoves = MAX_MOVES): Move[] {
  const copy = cloneGame(game);
  copy.turn = side;
  const moves: Move[] = [];
  for (const piece of copy.pieces) {
    if (piece.dead || piece.side !== side) continue;
    for (let row = 0; row < ROWS; row++) {
      for (let col = 0; col < COLS; col++) {
        if (isLegalMove(copy, piece.id, row, col)) {
          moves.push({ moveId: piece.id, captureId: pieceAt(copy, row, col), fromRow: piece.row, fromCol: piece.col, toRow: row, toCol: col });
          if (moves.length >= maxMoves) return moves;
        }
      }
    }
  }
  return moves;
}

function hasLegalMove(game: Game, side: Side): boolean { return generateMoves(game, side, 1).length > 0; }

function applyMove(game: Game, moveId: number, row: number, col: number): { game: Game; move: Move } | null {
  if (game.gameOver || !isLegalMove(game, moveId, row, col)) return null;
  const next = cloneGame(game);
  const move: Move = { moveId, captureId: pieceAt(next, row, col), fromRow: next.pieces[moveId].row, fromCol: next.pieces[moveId].col, toRow: row, toCol: col };
  fakeMove(next, move);
  next.history.push(move);
  if (next.pieces[4].dead || next.pieces[20].dead || !hasLegalMove(next, next.turn)) {
    next.gameOver = true;
    next.winner = otherSide(next.turn);
  }
  return { game: next, move };
}

function undoOne(game: Game): Game {
  const next = cloneGame(game);
  const move = next.history.pop();
  if (!move) return next;
  unfakeMove(next, move);
  next.gameOver = false;
  return next;
}

function positionScore(type: PieceType, row: number, col: number, side: Side): number {
  const forward = side === "red" ? 9 - row : row;
  const centerBonus = 4 - Math.abs(4 - col);
  switch (type) {
    case "horse": return centerBonus * 2 + (forward >= 3 && forward <= 6 ? 4 : 0);
    case "rook": return centerBonus + forward;
    case "cannon": return centerBonus * 2;
    case "soldier": return forward >= 5 ? 12 + centerBonus : forward * 2;
    default: return 0;
  }
}

function evaluate(game: Game, aiSide: Side, level: Difficulty): number {
  if (game.pieces[4].dead) return aiSide === "red" ? 100000 : -100000;
  if (game.pieces[20].dead) return aiSide === "black" ? 100000 : -100000;
  return game.pieces.reduce((score, piece) => {
    if (piece.dead) return score;
    let value = pieceValues[piece.type];
    if (level !== "easy") value += positionScore(piece.type, piece.row, piece.col, piece.side);
    return score + (piece.side === aiSide ? value : -value);
  }, 0);
}

function alphabeta(game: Game, depth: number, alphaValue: number, betaValue: number, aiSide: Side, level: Difficulty): number {
  let alpha = alphaValue, beta = betaValue;
  if (depth === 0 || game.gameOver) return evaluate(game, aiSide, level);
  const moves = generateMoves(game, game.turn);
  if (moves.length === 0) return game.turn === aiSide ? -90000 - depth : 90000 + depth;
  const maxing = game.turn === aiSide;
  let best = maxing ? Number.NEGATIVE_INFINITY : Number.POSITIVE_INFINITY;
  for (const move of moves) {
    fakeMove(game, move);
    const score = alphabeta(game, depth - 1, alpha, beta, aiSide, level);
    unfakeMove(game, move);
    if (maxing) { best = Math.max(best, score); alpha = Math.max(alpha, score); }
    else { best = Math.min(best, score); beta = Math.min(beta, score); }
    if (alpha >= beta) break;
  }
  return best;
}

function chooseAiMove(game: Game, level: Difficulty): Move | null {
  const moves = generateMoves(game, game.turn);
  if (!moves.length) return null;
  if (level === "easy" && Math.random() < 0.42) return moves[Math.floor(Math.random() * moves.length)];
  const depth = level === "easy" ? 1 : level === "medium" ? 2 : 3;
  const aiSide = game.turn;
  let bestScore = Number.NEGATIVE_INFINITY, bestMoves: Move[] = [];
  for (const move of moves) {
    const copy = cloneGame(game);
    fakeMove(copy, move);
    let score = alphabeta(copy, depth - 1, Number.NEGATIVE_INFINITY + 1, Number.POSITIVE_INFINITY - 1, aiSide, level);
    if (move.captureId !== -1) score += pieceValues[game.pieces[move.captureId].type] / 4;
    if (score > bestScore) { bestScore = score; bestMoves = [move]; }
    else if (score === bestScore) bestMoves.push(move);
  }
  return bestMoves[Math.floor(Math.random() * bestMoves.length)] || null;
}

function moveLabel(game: Game, move: Move): string {
  const piece = game.pieces[move.moveId];
  const files = "abcdefghi";
  return `${pieceText[piece.side][piece.type]} ${files[move.fromCol]}${10 - move.fromRow}-${files[move.toCol]}${10 - move.toRow}`;
}

function pieceAsset(piece: Piece): string {
  const prefix = piece.side === "red" ? "r" : "b";
  return `${PUBLIC_BASE}assets/xiangqi/${prefix}_${pieceAssetName[piece.type]}.png`;
}

function boardAsset(): string { return `${PUBLIC_BASE}assets/xiangqi/board.png`; }

function boardPosition(row: number, col: number, flipped: boolean): { left: string; top: string } {
  const displayRow = flipped ? ROWS - 1 - row : row;
  const displayCol = flipped ? COLS - 1 - col : col;
  return { left: `${((30 + displayCol * 60) / 540) * 100}%`, top: `${((30 + displayRow * 60) / 600) * 100}%` };
}

function activeCells(game: Game, selectedId: number, legalTargets: Set<string>, lastMove: Move | null, flipped: boolean) {
  const cells: Array<{ key: string; className: string; left: string; top: string }> = [];
  if (lastMove) {
    cells.push({ key: `last-from-${lastMove.fromRow}-${lastMove.fromCol}`, className: "board-marker last", ...boardPosition(lastMove.fromRow, lastMove.fromCol, flipped) });
    cells.push({ key: `last-to-${lastMove.toRow}-${lastMove.toCol}`, className: "board-marker last", ...boardPosition(lastMove.toRow, lastMove.toCol, flipped) });
  }
  if (selectedId >= 0) {
    const piece = game.pieces[selectedId];
    cells.push({ key: `selected-${piece.row}-${piece.col}`, className: "board-marker selected", ...boardPosition(piece.row, piece.col, flipped) });
  }
  legalTargets.forEach((target) => {
    const [row, col] = target.split(",").map(Number);
    cells.push({ key: `target-${row}-${col}`, className: "board-marker target", ...boardPosition(row, col, flipped) });
  });
  return cells;
}

function moveFromEngineToken(game: Game, token: string): Move | null {
  const parsed = parseUciMove(token);
  if (!parsed) return null;
  const moveId = pieceAt(game, parsed.fromRow, parsed.fromCol);
  if (moveId === -1) return null;
  const captureId = pieceAt(game, parsed.toRow, parsed.toCol);
  return { moveId, captureId, fromRow: parsed.fromRow, fromCol: parsed.fromCol, toRow: parsed.toRow, toCol: parsed.toCol };
}

function statusText(game: Game, mode: Mode, aiThinking: boolean): string {
  if (game.gameOver) return `Game over. ${game.winner === "red" ? "Red" : "Black"} wins.`;
  if (aiThinking) return "Opponent is thinking...";
  if (mode === "demo") return "AI demo running.";
  if (inCheck(game, game.turn)) return `${game.turn === "red" ? "Red" : "Black"} to move. Check.`;
  return `${game.turn === "red" ? "Red" : "Black"} to move.`;
}

function App() {
  const initial = useMemo(loadState, []);
  const [game, setGame] = useState<Game>(initial.game);
  const [savedGame, setSavedGame] = useState<Game | null>(initial.savedGame);
  const [mode, setMode] = useState<Mode>(initial.mode);
  const [difficulty, setDifficulty] = useState<Difficulty>(initial.difficulty);
  const [flipped, setFlipped] = useState(initial.flipped);
  const [selectedId, setSelectedId] = useState<number>(-1);
  const [aiThinking, setAiThinking] = useState(false);
  const [engineState, setEngineState] = useState<"loading" | "ready" | "fallback">("loading");
  const [engineDetail, setEngineDetail] = useState("");
  const [message, setMessage] = useState("");
  const [page, setPage] = useState<"game" | "about">("game");
  const [showHistory, setShowHistory] = useState(true);
  const [reviewIdx, setReviewIdx] = useState<number | null>(null);
  const engineRef = useRef<FairyStockfishEngine | null>(null);

  const totalMoves = game.history.length;
  const isReviewing = reviewIdx !== null;
  const displayIdx = reviewIdx ?? totalMoves;

  const displayGame = useMemo(() => {
    if (reviewIdx === null) return game;
    let g = createGame();
    const lim = Math.min(reviewIdx, game.history.length);
    for (let i = 0; i < lim; i++) {
      const m = game.history[i];
      const result = applyMove(g, m.moveId, m.toRow, m.toCol);
      if (result) g = result.game;
    }
    return g;
  }, [reviewIdx, game]);

  const legalTargets = useMemo(() => {
    if (reviewIdx !== null || selectedId < 0) return new Set<string>();
    const targets = new Set<string>();
    for (let row = 0; row < ROWS; row++)
      for (let col = 0; col < COLS; col++)
        if (isLegalMove(game, selectedId, row, col)) targets.add(`${row},${col}`);
    return targets;
  }, [game, selectedId, reviewIdx]);

  const lastMove = displayGame.history.length ? displayGame.history[displayGame.history.length - 1] : null;
  const humanTurn = !isReviewing && (mode === "two" || (mode === "red" && game.turn === "red") || (mode === "black" && game.turn === "black"));
  const markers = useMemo(() => activeCells(displayGame, isReviewing ? -1 : selectedId, isReviewing ? new Set<string>() : legalTargets, lastMove, flipped), [displayGame, selectedId, legalTargets, lastMove, flipped, isReviewing]);
  const livePieces = displayGame.pieces.filter((p) => !p.dead);

  function goReview(idx: number | null) {
    setReviewIdx(idx);
    if (idx !== null) setSelectedId(-1);
  }

  useEffect(() => {
    const state: PersistedState = { game, savedGame, mode, difficulty, flipped };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
  }, [game, savedGame, mode, difficulty, flipped]);

  useEffect(() => {
    const engine = new FairyStockfishEngine((state, detail) => {
      setEngineState(state);
      setEngineDetail(detail || "");
      if (state === "fallback" && detail) console.info(`Fairy-Stockfish unavailable: ${detail}`);
    });
    engineRef.current = engine;
    engine.init();
    return () => { engine.dispose(); engineRef.current = null; };
  }, []);

  useEffect(() => {
    if (game.gameOver) { setAiThinking(false); return; }
    const shouldAiMove = mode === "demo" || (mode === "red" && game.turn === "black") || (mode === "black" && game.turn === "red");
    if (!shouldAiMove) return;
    setAiThinking(true);
    let cancelled = false;
    const id = window.setTimeout(async () => {
      let result: { game: Game; move: Move } | null = null;
      const engineMove = await engineRef.current?.requestMove(game.history, difficulty);
      if (cancelled) return;
      if (engineMove) {
        const move = moveFromEngineToken(game, engineMove);
        if (move) result = applyMove(game, move.moveId, move.toRow, move.toCol);
      }
      if (!result) {
        const move = chooseAiMove(game, difficulty);
        if (move) result = applyMove(game, move.moveId, move.toRow, move.toCol);
      }
      if (!result) { setAiThinking(false); return; }
      setGame(result.game);
      setMessage(`${game.turn === "red" ? "Red" : "Black"}: ${moveLabel(game, result.move)}`);
      setSelectedId(-1);
      setAiThinking(false);
    }, mode === "demo" ? 160 : 260);
    return () => { cancelled = true; window.clearTimeout(id); };
  }, [difficulty, game, mode]);

  function newGame() {
    setGame(createGame());
    engineRef.current?.newGame();
    setSelectedId(-1);
    setAiThinking(false);
    setMessage("New game.");
    setReviewIdx(null);
  }

  function undoMove() {
    let next = undoOne(game);
    if (mode !== "two" && next.history.length > 0) next = undoOne(next);
    setGame(next);
    setSelectedId(-1);
    setAiThinking(false);
    setMessage("Move undone.");
    setReviewIdx(null);
  }

  function saveGame() { setSavedGame(cloneGame(game)); setMessage("Saved game in this browser."); }

  function loadGame() {
    if (!savedGame) { setMessage("No saved game yet."); return; }
    setGame(cloneGame(savedGame));
    setSelectedId(-1);
    setAiThinking(false);
    setMessage("Loaded saved game.");
    setReviewIdx(null);
  }

  function clearBrowserSave() { localStorage.removeItem(STORAGE_KEY); setSavedGame(null); newGame(); }

  function tapCell(row: number, col: number) {
    if (!humanTurn || aiThinking || game.gameOver) return;
    const id = pieceAt(game, row, col);
    if (selectedId >= 0 && legalTargets.has(`${row},${col}`)) {
      const result = applyMove(game, selectedId, row, col);
      if (!result) { setMessage("Illegal move."); return; }
      setGame(result.game);
      setMessage(`${game.turn === "red" ? "Red" : "Black"}: ${moveLabel(game, result.move)}`);
      setSelectedId(-1);
      return;
    }
    if (id !== -1 && game.pieces[id].side === game.turn) { setSelectedId(id); setMessage(`${pieceName[game.pieces[id].type]} selected.`); return; }
    setSelectedId(-1);
  }

  function tapBoard(event: React.MouseEvent<HTMLDivElement>) {
    if (!humanTurn || aiThinking || game.gameOver) return;
    const rect = event.currentTarget.getBoundingClientRect();
    const x = ((event.clientX - rect.left) / rect.width) * 540;
    const y = ((event.clientY - rect.top) / rect.height) * 600;
    let col = Math.round((x - 30) / 60);
    let row = Math.round((y - 30) / 60);
    if (flipped) { col = COLS - 1 - col; row = ROWS - 1 - row; }
    if (!inBounds(row, col)) return;
    tapCell(row, col);
  }

  return (
    <main className="app">
      <header className="hero">
        <h1>Exact Chinese Chess</h1>
        <p>{message || statusText(game, mode, aiThinking)}</p>
      </header>

      <section className="toolbar" aria-label="Game controls">
        <button onClick={newGame}>New</button>
        <button onClick={undoMove} disabled={game.history.length === 0 || aiThinking}>Undo</button>
        <button onClick={saveGame}>Save</button>
        <button onClick={loadGame}>Load</button>
        <button onClick={() => setFlipped((v) => !v)}>Flip</button>
        <button onClick={() => setShowHistory((v) => !v)} aria-pressed={showHistory}>
          {showHistory ? "Hide Moves" : "Show Moves"}
        </button>
        <button onClick={() => setPage((v) => (v === "game" ? "about" : "game"))}>{page === "game" ? "About" : "Game"}</button>
      </section>

      <section className="settings" aria-label="Settings">
        <label>Mode
          <select value={mode} onChange={(e) => setMode(e.target.value as Mode)}>
            <option value="red">Play Red</option>
            <option value="black">Play Black</option>
            <option value="two">2 Player</option>
            <option value="demo">AI Demo</option>
          </select>
        </label>
        <label>Level
          <select value={difficulty} onChange={(e) => setDifficulty(e.target.value as Difficulty)}>
            <option value="easy">Easy</option>
            <option value="medium">Medium</option>
            <option value="hard">Hard</option>
          </select>
        </label>
      </section>

      {page === "about" ? (
        <section className="about-page">
          <h2>About Exact Chinese Chess PWA</h2>
          <p>
            This is an installable browser version of Exact Chinese Chess. It keeps the grayscale, high-contrast visual
            direction from the e-ink native app while running entirely in the browser.
          </p>
          <p>
            Rules and the fallback AI are adapted from this project&apos;s native Xiangqi engine, which credits XMuli
            ChineseChess for GPL-3.0-or-later rules and AI lineage. Browser engine play uses Fairy-Stockfish WASM
            when the page is served with the isolation headers required by threaded WebAssembly.
          </p>
          <p>
            Attribution: XMuli ChineseChess, Augus1217 Chinese-Chess for artwork/reference lineage in the native app,
            official Pikafish for the optional native engine lineage, and GnomeGames4Kindle, originally by
            crazy-electron, for the e-ink/KUAL porting groundwork that informed the Exact apps.
          </p>
          <p>Licensing: keep this project under GPL-family terms. See the parent project&apos;s license and provenance files.</p>
          <button onClick={clearBrowserSave}>Clear Browser Save</button>
        </section>
      ) : (
        <section className={["play-area", showHistory ? "" : "history-hidden"].join(" ")}>
          <div className="xiangqi-board" aria-label="Xiangqi board" role="grid" onClick={tapBoard}>
            <img className="board-art" src={boardAsset()} alt="" draggable="false" />
            {markers.map((marker) => (
              <span key={marker.key} className={marker.className} style={{ left: marker.left, top: marker.top }} aria-hidden="true" />
            ))}
            {livePieces.map((piece) => (
              <img
                key={piece.id}
                className="piece"
                src={pieceAsset(piece)}
                alt={pieceText[piece.side][piece.type]}
                style={boardPosition(piece.row, piece.col, flipped)}
                draggable="false"
              />
            ))}
          </div>

          {showHistory && (
          <aside className="history">
            <h2>Moves</h2>
            <div className="review-nav">
              <button onClick={() => goReview(0)} disabled={displayIdx === 0} title="Start">◀◀</button>
              <button onClick={() => goReview(Math.max(0, displayIdx - 1))} disabled={displayIdx === 0} title="Previous">◀</button>
              <span className="review-label">{isReviewing ? `${displayIdx}/${totalMoves}` : "Live"}</span>
              <button onClick={() => displayIdx < totalMoves ? goReview(displayIdx + 1) : goReview(null)} disabled={displayIdx >= totalMoves} title="Next">▶</button>
              <button onClick={() => goReview(null)} disabled={!isReviewing} title="Live">▶▶</button>
            </div>
            <ol>
              {game.history.map((move, index) => (
                <li
                  key={`${index}-${move.moveId}-${move.toRow}-${move.toCol}`}
                  className={displayIdx === index + 1 ? "active" : ""}
                  onClick={() => goReview(index + 1)}
                >{moveLabel(game, move)}</li>
              ))}
            </ol>
          </aside>
          )}
        </section>
      )}

      <footer className="notes">
        <p>
          Auto-continue is always on. Save/Load creates a manual browser restore point. Engine:{" "}
          {engineState === "ready" ? "Fairy-Stockfish WASM" : engineState === "loading" ? "loading Fairy-Stockfish" : "built-in fallback AI"}.
          {engineState === "fallback" && engineDetail ? ` ${engineDetail}` : ""}
          {engineState !== "ready" ? ` crossOriginIsolated=${String(window.crossOriginIsolated)}` : ""}
        </p>
      </footer>
    </main>
  );
}

export default App;
