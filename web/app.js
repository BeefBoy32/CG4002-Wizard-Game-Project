// app.js — Wizard Game Live UI (MQTT over WebSockets)

// ---------- config ----------
const HOST = location.hostname || "127.0.0.1"; // change if broker is on another machine
const PORT = 9001;                              // your Mosquitto WebSocket listener
const PROTO = "ws";                             // use "wss" if you enable TLS for websockets

// ---------- topics ----------
const TOP = {
    U96_STATUS: "u96/status",
    W1_BATT: "wand1/batt",
    W2_BATT: "wand2/batt",
    W1_SPELL: "u96/wand1/spell",
    W2_SPELL: "u96/wand2/spell",
    W1_STATUS: "wand1/status",
    W2_STATUS: "wand2/status",
    GAME: "u96/game",
    GAME_END: "u96/game_end",
};

// prettier spell symbols
const GLYPH = { W: "〰️", C: "⭕", S: "⬜", T: "🔺", Z: "⚡", I: "♾️", U: "·" };
const NAME = { W: "Wave", C: "Circle", S: "Square", T: "Triangle", Z: "Zigzag", I: "Infinity", U: "Unknown" };
const glyphFor = (l) => GLYPH[l] ?? "·";


// ---------- helpers ----------
const $ = (id) => document.getElementById(id);

function setHP(el, v) {
    el.textContent = v;
    el.className = "spell " + (v >= 2 ? "ok" : v === 1 ? "warn" : "bad");
}

function setBatt(el, pct) {
    el.textContent = `${pct ?? "--"}%`;
    const v = Number(pct);
    el.className = "spell " + (v >= 50 ? "ok" : v >= 20 ? "warn" : "bad");
}

function toStrengthLevel(s) {
    const v = Number(s);
    if (!Number.isFinite(v)) return 1;
    if (v <= 5) return Math.min(5, Math.max(1, Math.round(v)));
    // percent-style → 5 buckets
    if (v >= 80) return 5;
    if (v >= 60) return 4;
    if (v >= 40) return 3;
    if (v >= 20) return 2;
    return 1;
}

function playerClass(pid) {
    return pid === 1 ? "p1" : "p2";
}


function setSpellDisplay(el, letter, strength, pid) {
    const lvl = toStrengthLevel(strength);
    el.textContent = letter || "U";
    el.className = `spell ${playerClass(pid)} spell-lv${lvl}`;
}


// Render the 5 battlefield cells.
// lanes[i] = [playerId, letter, strength] or null
function renderBoard(lanes = []) {
    const board = $("board");
    if (!board) return;

    const cells = Array.from(board.querySelectorAll(".cell"));
    for (let i = 0; i < cells.length; i++) {
        const cell = cells[i];
        const slot = lanes[i];

        if (Array.isArray(slot)) {
            const pid = slot[0];
            const letter = slot[1];
            const strength = slot[2];
            const lvl = toStrengthLevel(strength);

            cell.textContent = glyphFor(letter);                       // show symbol
            cell.title = `${NAME[letter] || "Spell"} • Strength: ${strength ?? lvl}`;
            cell.className = `cell ${playerClass(pid)} spell-lv${lvl}`; // size still from strength
            cell.style.transition = "font-size 120ms ease";
        } else {
            cell.textContent = "";
            cell.className = "cell";
            cell.style.transition = "";
        }
    }
}

function showOverlay(message, winner) {
    const ov = document.getElementById("overlay");
    const title = document.getElementById("overlayTitle");
    const sub = document.getElementById("overlaySub");
    title.textContent = message || "Game Over";
    title.classList.remove("winner-1", "winner-2");
    if (winner === 1 || winner === 2) title.classList.add(`winner-${winner}`);
    sub.textContent = "Press ESC to close";
    ov.classList.add("show");
}
function hideOverlay() { document.getElementById("overlay")?.classList.remove("show"); }
document.addEventListener("keydown", (e) => { if (e.key === "Escape") hideOverlay(); });


// ---------- main ----------
document.addEventListener("DOMContentLoaded", () => {
    // build WS URL
    const url = `${PROTO}://${HOST}:${PORT}`;

    // connect
    const client = mqtt.connect(url, {
        clientId: "web-" + Math.random().toString(16).slice(2),
        clean: true,
        reconnectPeriod: 1000,
    });

    client.on("connect", () => {
        $("connDot").className = "dot on";
        $("connTxt").textContent = "connected";

        client.subscribe([
            TOP.U96_STATUS,
            TOP.W1_BATT, TOP.W2_BATT,
            TOP.W1_SPELL, TOP.W2_SPELL,
            TOP.GAME, TOP.GAME_END,
            TOP.W1_STATUS, TOP.W2_STATUS,
        ]);
    });

    client.on("reconnect", () => {
        $("connDot").className = "dot off";
        $("connTxt").textContent = "reconnecting…";
    });
    client.on("close", () => {
        $("connDot").className = "dot off";
        $("connTxt").textContent = "disconnected";
    });

    client.on("message", (topic, payload) => {
        let js;
        try {
            js = JSON.parse(new TextDecoder().decode(payload));
        } catch {
            return; // ignore non-JSON
        }

        switch (topic) {
            case TOP.U96_STATUS:
                $("u96Ready").textContent = js.ready;
                break;

            case TOP.W1_STATUS:
                $("w1Ready").textContent = js.ready;
                break;

            case TOP.W2_STATUS:
                $("w2Ready").textContent = js.ready;
                break;

            case TOP.W1_BATT:
                setBatt($("batt1"), js.percent);
                break;

            case TOP.W2_BATT:
                setBatt($("batt2"), js.percent);
                break;

            case TOP.W1_SPELL: {
                const L = js.spell_type || "U";
                $("spell1").textContent = glyphFor(L);
                $("spell1").title = NAME[L] || "Spell";
                break;
            }

            case TOP.W2_SPELL: {
                const L = js.spell_type || "U";
                $("spell2").textContent = glyphFor(L);
                $("spell2").title = NAME[L] || "Spell";
                break;
            }

            case TOP.GAME:
                // { p1_health, p2_health, lanes:[...], ts }
                setHP($("p1hp"), js.player1_hp);
                setHP($("p2hp"), js.player2_hp);
                renderBoard(js.battlefield);
                $("result").textContent = "";
                break;

            case TOP.GAME_END: {
                const winner = js?.winner;
                const msg = winner ? `Game Over — Player ${winner} wins!` : "Game Over";
                document.getElementById("result").textContent = msg; // keep small text if you want
                showOverlay(msg, winner);
                break;
            }
        }
    });
});
