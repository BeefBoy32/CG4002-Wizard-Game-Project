import socket, json, time, threading, sys
from collections import deque

HOST = "127.0.0.1"      # bind all
PORT = 5000


# Global variables for game logic
# Locks to be acquired when modifying spell deque
spells_lock = threading.Lock()
display_lock = threading.Lock()
player1_spells = deque(maxlen=5)
player2_spells = deque(maxlen=5)
UPDATE_INTERVAL = 0.5
battery_percent1 = 100
battery_percent2 = 100

WINDOW = 60
count = 0

'''
data: dict of wand_id, spell_type and strength
'''
def add_player_spell(data):
    with spells_lock:
        if data["wand_id"] == 0:
            if len(player1_spells) >= 5:
                print("Too many spells from player 1! Discarding spell...")
                return
            if len(player1_spells) >= 1 and time.time() - player1_spells[-1]["time"] < 2.0:
                print("Player 1 Spamming spells... Spell discarded")
                return
            player1_spells.append({"spell_type": data["spell_type"], "strength": data["strength"], "time": time.time()})
            return
        if len(player2_spells) >= 5:
            print("Too many spells from player 2! Discarding spell...")
            return
        if len(player2_spells) >= 1 and time.time() - player2_spells[-1]["time"] < 2.0:
            print("Player 2 Spamming spells... Spell discarded")
            return
        player2_spells.append({"spell_type": data["spell_type"], "strength": data["strength"], "time": time.time()})

'''
'''
def weakenSpell(strength1, strength2):
    if strength1 > strength2:
        player1_spells[0]["strength"] -= strength2
    else:
        player2_spells[0]["strength"] -= strength1

'''
Return: 1 if player 1 win, 2 if player 2 win, 0 if draw
'''
def getCollidingSpellWinner():
    player1_spell = player1_spells[0]["spell_type"]
    player2_spell = player2_spells[0]["spell_type"]
    match (player1_spell):
        # Wave
        # Wins: Square and Circle, Draws: ZigZag, Lose: Infinity, Triangle
        case "W":
            if player2_spell == "S" or player2_spell == "C":
                return 1
            elif player2_spell == "W" or player2_spell == "Z":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                weakenSpell(strength1, strength2)
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return 2
        # Circle
        # Wins: Square and Triangle, Draws: Infinity, Lose: Wave, ZigZag 
        case "C":
            if player2_spell == "S" or player2_spell == "T":
                return 1
            elif player2_spell == "I" or player2_spell == "C":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                weakenSpell(strength1, strength2)
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return 2
        # Square
        # Wins: Infinity and Lightning, Draws: Triangle, Lose: Wave, Circle
        case "S":
            if player2_spell == "I" or player2_spell == "L":
                return 1
            elif player2_spell == "T" or player2_spell == "S":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                weakenSpell(strength1, strength2)
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return 2
            
        # Triangle
        # Wins: ZigZag and Wave, Draws: Square, Lose: Infinity, Circle
        case "T":
            if player2_spell == "Z" or player2_spell == "W":
                return 1
            elif player2_spell == "S" or player2_spell == "T":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                weakenSpell(strength1, strength2)
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return 2
            
        # ZigZag
        # Wins: Infinity and Circle, Draws: Wave, Lose: Triangle, Square
        case "Z":
            if player2_spell == "I" or player2_spell == "C":
                return 1
            elif player2_spell == "Z" or player2_spell == "W":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                weakenSpell(strength1, strength2)
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return 2
            
        # Infinity
        # Wins: Triangle and Wave, Draws: Circle, Lose: ZigZag, Square
        case "I":
            if player2_spell == "T" or player2_spell == "W":
                return 1
            elif player2_spell == "I" or player2_spell == "C":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                weakenSpell(strength1, strength2)
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return 2
            
        case _:
            print("Unknown spell type, Game end!")
            return None
    

def calculatePosition(time, currentTime, player):
    position = int((currentTime - time) / 2)
    return position if player else 4 - position

def getDisplayFromSpells(player1_health, player2_health, spell_display):
    image = f"P1:{player1_health}"
    for spell in spell_display:
        if spell:
            image += spell
        else:
            image += "    "
    image += f"P2:{player2_health}"
    return image

def modifyBatt(wand_num):
    with display_lock:
        if wand_num == 1:
            sys.stdout.write("\033[1B")
            sys.stdout.write("\033[2K")
            print(f"Wand1 Battery%: {battery_percent1}", end = "\r")
            sys.stdout.write("\033[1A")
        elif wand_num == 2:
            sys.stdout.write("\033[2B")
            sys.stdout.write("\033[2K")
            print(f"Wand2 Battery%: {battery_percent2}", end = "\r")
            sys.stdout.write("\033[2A")

def game_loop():
    player1_health = 3
    player2_health = 3
    gameEnd = False
    initial_spell_display = [None] * 5
    image = getDisplayFromSpells(player1_health, player2_health, initial_spell_display)
    print(image, end = "\n")
    print(f"Wand1 Battery%: {battery_percent1}")
    print(f"Wand2 Battery%: {battery_percent2}")
    sys.stdout.write("\033[3A")
    sys.stdout.write("\r") 
    sys.stdout.flush()

    while not gameEnd:
        currentTime  = time.time()
        spell_display = [None] * 5

        with spells_lock:
            # Check and eliminate loser of colliding spells
            if len(player1_spells) and len(player2_spells) and calculatePosition(player1_spells[0]["time"], currentTime, True) >= calculatePosition(player2_spells[0]["time"], currentTime, False):
                winner = getCollidingSpellWinner()
                if winner == 1:
                    player2_spells.popleft()
                elif winner == 2:
                    player1_spells.popleft()
                else:
                    player2_spells.popleft()
                    player1_spells.popleft()

            # Check if player 1 deals damage
            if len(player1_spells) and calculatePosition(player1_spells[0]["time"], currentTime, True) >= 5:
                player1_spells.popleft()
                player2_health -= 1
                if player2_health <= 0:
                    gameEnd = True

            # Check if player 2 deals damage
            elif len(player2_spells) and calculatePosition(player2_spells[0]["time"], currentTime, False) <= -1:
                player2_spells.popleft()
                player1_health -= 1
                if player1_health <= 0:
                    gameEnd = True

        for spell_info in player2_spells:
            spellType = spell_info["spell_type"]
            spellPosition = calculatePosition(spell_info["time"], currentTime, False)
            spell_display[spellPosition] = f"{spellType}, {spell_info['strength']}"  
        
        for spell_info in player1_spells:
            spellType = spell_info["spell_type"]
            spellPosition = calculatePosition(spell_info["time"], currentTime, True)
            spell_display[spellPosition] = f"{spellType}, {spell_info['strength']}" 

        image = getDisplayFromSpells(player1_health, player2_health, spell_display)

        if not gameEnd:
            with display_lock:
                print(image, end = "\r")
                sys.stdout.flush()

        time.sleep(UPDATE_INTERVAL)
    
    print(f"Game End: Player {1 if player2_health == 0 else 2}")

def main():
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((HOST, PORT))
    server_sock.listen(1)
    print(f"Ultra96 server listening on port {PORT}...")

    conn, addr = server_sock.accept()
    print(f"Connection from {addr}")

    threading.Thread(target=game_loop, daemon=True).start()
    with conn:
        buf = b""
        while True:
            data = conn.recv(4096)
            if not data:
                print("Client closed connection.")
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if not line.strip():
                    continue
                try:
                    global count
                    count += 1
                    if count == WINDOW:
                        result = {
                        "wand_id": 1,
                        "spell_id": 1,        # keep if you still use numeric somewhere
                        "spell_type": "C",    # <-- single-letter for ESP
                        "conf": 0.97,
                        "stable": True
                        }
                        conn.sendall((json.dumps(result) + "\n").encode())
                        count = 0
                except Exception as e:
                    print("Bad window JSON:", e)

    server_sock.close()

if __name__ == "__main__":
    main()