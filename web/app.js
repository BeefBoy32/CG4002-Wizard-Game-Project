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
    GAME: "u96/game",
    GAME_END: "u96/game_end",
};

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

// Render the 5 battlefield cells with just the spell letter (left→right)
function renderBoard(lanes) {
    const board = $("board");
    if (!board) return;
    const cells = Array.from(board.querySelectorAll(".cell"));
    for (let i = 0; i < cells.length; i++) {
        const slot = lanes?.[i];
        // slot is like "W, 3" → show just the letter
        cells[i].textContent = slot ? String(slot).split(",")[0] : "";
    }
}

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
                $("w1Draw").textContent = js.wand1_state?.drawingMode ?? false;
                $("w2Draw").textContent = js.wand2_state?.drawingMode ?? false;
                break;

            case TOP.W1_BATT:
                setBatt($("batt1"), js.percent);
                break;

            case TOP.W2_BATT:
                setBatt($("batt2"), js.percent);
                break;

            case TOP.W1_SPELL:
                $("spell1").textContent = js.spell_type || "U";
                break;

            case TOP.W2_SPELL:
                $("spell2").textContent = js.spell_type || "U";
                break;

            case TOP.GAME:
                // { p1_health, p2_health, lanes:[...], ts }
                setHP($("p1hp"), js.p1_health);
                setHP($("p2hp"), js.p2_health);
                renderBoard(js.lanes);
                $("result").textContent = "";
                break;

            case TOP.GAME_END:
                $("result").textContent = js?.winner
                    ? `Game Over — Player ${js.winner} wins!`
                    : "Game Over";
                break;
        }
    });
});
