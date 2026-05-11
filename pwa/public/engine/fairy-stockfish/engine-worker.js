let engine = null;
let initialized = false;
let ready = false;

function sendToApp(message) {
  self.postMessage(message);
}

function enginePath(file) {
  return new URL(file, self.location.href).href;
}

function skillFor(level) {
  if (level === "easy") return { skill: -12, elo: 500, movetime: 90 };
  if (level === "hard") return { skill: 12, elo: 1900, movetime: 420 };
  return { skill: 2, elo: 1050, movetime: 220 };
}

function postEngine(command) {
  if (engine) engine.postMessage(command);
}

function configure(level) {
  const settings = skillFor(level);
  postEngine("setoption name UCI_Variant value xiangqi");
  postEngine("setoption name Ponder value false");
  postEngine("setoption name UCI_LimitStrength value true");
  postEngine(`setoption name Skill Level value ${settings.skill}`);
  postEngine(`setoption name UCI_Elo value ${settings.elo}`);
}

function handleEngineLine(line) {
  sendToApp({ type: "line", line });
  if (line === "uciok") {
    configure("medium");
    postEngine("isready");
    return;
  }
  if (line === "readyok") {
    ready = true;
    sendToApp({ type: "ready" });
    return;
  }
  const match = /^bestmove\s+(\S+)/.exec(line);
  if (match) sendToApp({ type: "bestmove", move: match[1] });
}

async function initEngine() {
  if (initialized) return;
  initialized = true;
  if (typeof SharedArrayBuffer === "undefined") {
    sendToApp({ type: "unavailable", reason: "SharedArrayBuffer unavailable. Serve with COOP/COEP headers to enable Fairy-Stockfish WASM." });
    return;
  }
  try {
    importScripts("./stockfish.js");
    if (typeof Stockfish !== "function") throw new Error("Stockfish factory missing");
    engine = await Stockfish({
      locateFile: enginePath,
      mainScriptUrlOrBlob: enginePath("stockfish.js")
    });
    engine.addMessageListener(handleEngineLine);
    postEngine("uci");
  } catch (error) {
    sendToApp({ type: "unavailable", reason: error instanceof Error ? error.message : String(error) });
  }
}

self.addEventListener("message", (event) => {
  const data = event.data || {};
  if (data.type === "init") {
    initEngine();
    return;
  }
  if (!engine || !ready) {
    sendToApp({ type: "waiting", reason: "Engine is not ready." });
    return;
  }
  if (data.type === "newgame") {
    postEngine("ucinewgame");
    return;
  }
  if (data.type === "go") {
    const settings = skillFor(data.level);
    configure(data.level);
    postEngine("isready");
    const moves = Array.isArray(data.moves) && data.moves.length ? ` moves ${data.moves.join(" ")}` : "";
    postEngine(`position startpos${moves}`);
    postEngine(`go movetime ${settings.movetime}`);
  }
});
