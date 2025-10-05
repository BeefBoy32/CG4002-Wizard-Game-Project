import socket, json
import threading
import time, sys
from collections import deque

HOST = ""      # bind all
PORT = 5000


# Global variables for game logic
# Locks to be acquired when modifying spell deque
spells_lock = threading.Lock()
display_lock = threading.Lock()
player1_spells = deque(maxlen=5)
player2_spells = deque(maxlen=5)
UPDATE_INTERVAL = 0.5
battery_percent = 100


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
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return False
        # Circle
        # Wins: Square and Triangle, Draws: Infinity, Lose: Wave, ZigZag 
        case "C":
            if player2_spell == "S" or player2_spell == "T":
                return 1
            elif player2_spell == "I" or player2_spell == "C":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return False

        # Square
        # Wins: Infinity and Lightning, Draws: Triangle, Lose: Wave, Circle
        case "S":
            if player2_spell == "I" or player2_spell == "L":
                return 1
            elif player2_spell == "T" or player2_spell == "S":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return False
            
        # Triangle
        # Wins: ZigZag and Wave, Draws: Square, Lose: Infinity, Circle
        case "T":
            if player2_spell == "Z" or player2_spell == "W":
                return 1
            elif player2_spell == "S" or player2_spell == "T":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return False
            
        # ZigZag
        # Wins: Infinity and Circle, Draws: Wave, Lose: Triangle, Square
        case "Z":
            if player2_spell == "I" or player2_spell == "C":
                return 1
            elif player2_spell == "Z" or player2_spell == "W":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return False
            
        # Infinity
        # Wins: Triangle and Wave, Draws: Circle, Lose: ZigZag, Square
        case "I":
            if player2_spell == "T" or player2_spell == "W":
                return 1
            elif player2_spell == "I" or player2_spell == "C":
                strength1 = player1_spells[0]["strength"]
                strength2 = player2_spells[0]["strength"]
                if strength1 > strength2:
                    return 1
                if strength1 == strength2:
                    return 0
                return 2
            else:
                return False
            
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

def modifyBatt():
    with display_lock:
        sys.stdout.write("\033[1B")
        sys.stdout.write("\033[2K")
        print(f"Battery%: {battery_percent}", end = "\r")
        sys.stdout.write("\033[1A")

def game_loop():
    player1_health = 3
    player2_health = 3
    gameEnd = False
    initial_spell_display = [None] * 5
    image = getDisplayFromSpells(player1_health, player2_health, initial_spell_display)
    print(image, end = "\n")
    print(f"Battery%: {battery_percent}")
    sys.stdout.write("\033[2A")
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

def dummy_loop():
    time.sleep(5.0)
    json_data = '{"wand_id": 0, "spell_type": "I", "strength": 5}'
    data_dict = json.loads(json_data)
    add_player_spell(data_dict)
    json_data = '{"wand_id": 1, "spell_type": "I", "strength": 4}'
    data_dict = json.loads(json_data)
    add_player_spell(data_dict)
    global battery_percent
    battery_percent = 99
    modifyBatt()
    while True:
        continue
    

if __name__ == "__main__":
    threading.Thread(target=game_loop, daemon=True).start()
    dummy_loop()