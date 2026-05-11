export type Side = "red" | "black";
export type PieceType = "general" | "advisor" | "elephant" | "horse" | "rook" | "cannon" | "soldier";
export type Mode = "red" | "black" | "two" | "demo";
export type Difficulty = "easy" | "medium" | "hard";

export type Piece = {
  id: number;
  row: number;
  col: number;
  dead: boolean;
  side: Side;
  type: PieceType;
};

export type Move = {
  moveId: number;
  captureId: number;
  fromRow: number;
  fromCol: number;
  toRow: number;
  toCol: number;
};

export type Game = {
  pieces: Piece[];
  history: Move[];
  turn: Side;
  gameOver: boolean;
  winner: Side;
};
