# from socket import socket
import json, time, collections, threading, socket, os, sys
import paho.mqtt.client as mqtt
from collections import deque
from ai_engine import infer as ai_infer, class_to_letter
import numpy as np

BROKER = "localhost"   # "localhost" if shift to ultra96, laptopIP if on laptop
PORT   = 1883            # your mosquitto port


# Global variables for testing 
WINDOW = 60
COUNT1 = 0
COUNT2 = 0
predictionCount = 0
DEBUG_AI = True
# format [yaw, pitch, roll, accelx, accely, accelz]
buf = {
    1: deque(maxlen=WINDOW),
    2: deque(maxlen=WINDOW)
}
cli = None #MQTT client

# Global variables for game logic
# Locks to be acquired when modifying spell deque
connected = threading.Event()
pauseState = threading.Event()
gameReady = threading.Event()
gameEnd = threading.Event()
pause = threading.Semaphore(0)
pausedTime = 0
wand1IsReady = threading.Event()
wand2IsReady = threading.Event()
mpu1_lock = threading.Lock()
mpu2_lock = threading.Lock()
spells_lock = threading.Lock()
display_lock = threading.Lock()
pauseLock = threading.Lock()
player1_spells = deque(maxlen=5)
player2_spells = deque(maxlen=5)
UPDATE_INTERVAL = 0.5
battery_percent1 = None
battery_percent2 = 100.0
wand1_drawingMode = threading.Event()   
wand2_drawingMode = threading.Event()
AIModelUsed = threading.Lock()
wand1_spell = "U"
wand2_spell = "U"

# Topics
# Subscribed topics
T_WAND1_STATUS = "wand1/status" 
T_WAND2_STATUS = "wand2/status"
T_WAND1_BATT = "wand1/batt"
T_WAND2_BATT = "wand2/batt"
T_WAND1_MPU = "wand1/mpu"
T_WAND2_MPU = "wand2/mpu"
T_WAND1_CAST = "wand1/cast"
T_WAND2_CAST = "wand2/cast"

# Topic to publish to
T_U96_STATUS = "u96/status"
T_U96_WAND1_SPELL = "u96/wand1/spell"
T_U96_WAND2_SPELL = "u96/wand2/spell"



def on_connect(client, userdata, flags, rc):
    client.subscribe([
        (T_WAND1_STATUS, 1),
        (T_WAND2_STATUS, 1),
        (T_WAND1_BATT, 1),
        (T_WAND2_BATT, 1),
        (T_WAND1_MPU, 0),
        (T_WAND2_MPU, 0),
        (T_WAND1_CAST, 2),
        (T_WAND2_CAST, 2),
    ])
    connected.set()

def on_disconnect(client, userdata, rc):
    connected.clear()
    pause.release()

# MQTT message handler
def on_message(client, _, msg):
    global battery_percent1, battery_percent2, wand1_spell, wand2_spell
    topic = msg.topic
    try:
        msgJS = json.loads(msg.payload.decode())
        #print(msg.payload.decode(), topic)
        if topic == T_WAND1_STATUS:
            if msgJS["ready"]:
                wand1IsReady.set()
            else: 
                wand1IsReady.clear()
                pause.release()

        if topic == T_WAND2_STATUS:
            if msgJS["ready"]:
                wand2IsReady.set()
            else: 
                wand2IsReady.clear()
                pause.release() 

        elif topic == T_WAND1_MPU:
            # TODO add to wand1_MPU deque
            if gameReady.is_set() and wand1_drawingMode.is_set():
                with mpu1_lock:
                    buf[1].append([float(msgJS["yaw"]), float(msgJS["pitch"]), float(msgJS["roll"]),
                                float(msgJS["accelx"]), float(msgJS["accely"]), float(msgJS["accelz"])])

        elif topic == T_WAND2_MPU:
            # TODO add to wand2_MPU deque
            if gameReady.is_set() and wand2_drawingMode.is_set():
                with mpu2_lock:
                    buf[2].append([float(msgJS["yaw"]), float(msgJS["pitch"]), float(msgJS["roll"]),
                                float(msgJS["accelx"]), float(msgJS["accely"]), float(msgJS["accelz"])])

        elif topic == T_WAND1_BATT:            
            battery_percent1 = msgJS["percent"]
            # if gameReady.is_set():
                #modifyBatt(1)
                

        elif topic == T_WAND2_BATT:
            battery_percent2 = msgJS["percent"]
            # if gameReady.is_set():
                #modifyBatt(2)

        elif topic == T_WAND1_CAST:
            with mpu1_lock:
                if len(buf[1]):
                    buf[1].clear()
            add_player_spell(msgJS, 1)
            wand1_drawingMode.set()

        elif topic == T_WAND2_CAST:
            with mpu2_lock:
                if len(buf[2]):
                    buf[2].clear() 
            add_player_spell(msgJS, 2)
            wand2_drawingMode.set()
            
    except UnicodeDecodeError:
        print(f"[WARN] Non-text payload on topic {msg.topic}: {msg.payload}")
        return
    except json.JSONDecodeError:
        print(f"[WARN] Invalid JSON on topic {msg.topic}: {msg.payload}")
        return
    

'''
data: dict of wand_id, spell_type and strength
'''
def add_player_spell(data, playerID):
    with spells_lock:
        if playerID == 1:
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
        return

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
    initial_spell_display = [None] * 5
    image = getDisplayFromSpells(player1_health, player2_health, initial_spell_display)
    print(image, end = "\n")
    print(f"Wand1 Battery%: {battery_percent1}")
    print(f"Wand2 Battery%: {battery_percent2}")
    sys.stdout.write("\033[3A")
    sys.stdout.write("\r") 
    sys.stdout.flush()

    while not gameEnd.is_set():
        gameReady.wait()
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
                    gameEnd.set()

            # Check if player 2 deals damage
            elif len(player2_spells) and calculatePosition(player2_spells[0]["time"], currentTime, False) <= -1:
                player2_spells.popleft()
                player1_health -= 1
                if player1_health <= 0:
                    gameEnd.set()

        for spell_info in player2_spells:
            spellType = spell_info["spell_type"]
            spellPosition = calculatePosition(spell_info["time"], currentTime, False)
            spell_display[spellPosition] = f"{spellType}, {spell_info['strength']}"  
        
        for spell_info in player1_spells:
            spellType = spell_info["spell_type"]
            spellPosition = calculatePosition(spell_info["time"], currentTime, True)
            spell_display[spellPosition] = f"{spellType}, {spell_info['strength']}" 

        image = getDisplayFromSpells(player1_health, player2_health, spell_display)

        if not gameEnd.is_set():
            with display_lock:
                print(image, end = "\r")
                sys.stdout.flush()

        time.sleep(UPDATE_INTERVAL)

    print(f"Game End: Player {1 if player2_health == 0 else 2}")

def checkPauseLoop():
    while True:
        pause.acquire()
        pauseLock.acquire()
        global pausedTime
        if (not(wand2IsReady.is_set() and wand1IsReady.is_set() and connected.is_set())):       
            gameReady.clear()
            pausedTime = time.time()
            message = {
                "ready":connected.is_set(),
                "wand1_state": {
                    "drawingMode":wand1_drawingMode.is_set(),
                    "spell":wand1_spell,
                },
                "wand2_state": {
                    "drawingMode":wand2_drawingMode.is_set(),
                    "spell":wand2_spell,
                }
            }
            cli.publish(T_U96_STATUS, json.dumps(message), 1, True)
            pauseState.set()
        else:
            pauseLock.release()

def updateTimeLoop():
    while True:
        pauseState.wait()
        wand1IsReady.wait() 
        wand2IsReady.wait()
        connected.wait()
        with mpu1_lock:
                if len(buf[1]):
                    buf[1].clear()
        with mpu2_lock:
            if len(buf[2]):
                buf[2].clear()
        unPauseTime = time.time()
        timeDiff = unPauseTime - pausedTime
        with spells_lock:
            for spell_info in player1_spells:
                spell_info["time"] += timeDiff
                
            for spell_info in player2_spells:
                spell_info["time"] += timeDiff
        gameReady.set()
        pauseState.clear()
        pauseLock.release()
        # Maybe have another event flag to trigger game logic

def AILoopPlayer1():
    while True:
        global predictionCount
        gameReady.wait()
        wand1_drawingMode.wait()
        if len(buf[1]) == WINDOW:
            with mpu1_lock:
                window = np.array[buf[1]] # (60,6)
            with AIModelUsed:
                out = ai_infer(window)

            # normalize ai_infer output to a class index + optional vector
            pred_idx = None
            vec = None
            if isinstance(out, tuple):
                # if first element is logits/probs vector, use argmax of that
                first = out[0]
                if isinstance(first, (list, np.ndarray)) and np.size(first) > 1:
                    vec = np.array(first, dtype=np.float32).reshape(-1)
                    pred_idx = int(np.argmax(vec))
                else:
                    pred_idx = int(first)
            elif isinstance(out, (list, np.ndarray)):
                vec = np.array(out, dtype=np.float32).reshape(-1)
                pred_idx = int(np.argmax(vec))
            else:
                pred_idx = int(out)
            letter = class_to_letter(pred_idx)
            predictionCount += 1
            print(f"Prediction {predictionCount} made: {letter}")
            for data in window:
                print(data)
            '''
            if letter != 'U':
                message = {        # keep if you still use numeric somewhere
                    "spell_type":letter,    # <-- single-letter for ESP
                }
                cli.publish(T_U96_WAND1_SPELL, json.dumps(message), 2, False)
                wand1_drawingMode.clear()
                with mpu1_lock:
                    # reset mpu data
                    buf[1].clear()
            '''

def AILoopPlayer2():
    while True:
        gameReady.wait()
        wand2_drawingMode.wait()
        if len(buf[2]) == WINDOW:
            with mpu2_lock:
                window = np.array(buf[2]) # (60,6)
            with AIModelUsed:
                out = ai_infer(window)

            # normalize ai_infer output to a class index + optional vector
            pred_idx = None
            vec = None
            if isinstance(out, tuple):
                # if first element is logits/probs vector, use argmax of that
                first = out[0]
                if isinstance(first, (list, np.ndarray)) and np.size(first) > 1:
                    vec = np.array(first, dtype=np.float32).reshape(-1)
                    pred_idx = int(np.argmax(vec))
                else:
                    pred_idx = int(first)
            elif isinstance(out, (list, np.ndarray)):
                vec = np.array(out, dtype=np.float32).reshape(-1)
                pred_idx = int(np.argmax(vec))
            else:
                pred_idx = int(out)
            letter = class_to_letter(pred_idx)

            if letter != 'U':
                message = {        # keep if you still use numeric somewhere
                    "spell_type":letter,    # <-- single-letter for ESP
                }
                cli.publish(T_U96_WAND2_SPELL, json.dumps(message), 2, False)
                wand2_drawingMode.clear()
                with mpu2_lock:
                    # reset mpu data
                    buf[2].clear()


def main():
    global cli
    cli = mqtt.Client("Ultra96-client")
    # --- set Last Will before connecting ---
    will_message = json.dumps({
        "ready":False,
        "wand1_state": {
            "drawingMode":wand1_drawingMode.is_set(),
            "spell":wand1_spell,
        },
        "wand2_state": {
            "drawingMode":wand2_drawingMode.is_set(),
            "spell":wand2_spell,
        }
    })
    # topic, payload, qos, retain
    cli.will_set(T_U96_STATUS, payload=will_message, qos=1, retain=True)
    cli.on_connect = on_connect
    cli.on_message = on_message
    cli.on_disconnect = on_disconnect
    cli.connect(BROKER, PORT, 60)
    print("Waiting for connection...")
    cli.loop_start()
    wand1_drawingMode.set()
    wand2_drawingMode.set()
    message = {"ready":True}
    cli.publish(T_WAND2_STATUS, json.dumps(message), 1, True)
    wand2IsReady.set()
    wand1IsReady.wait()
    wand2IsReady.wait()
    while(not(battery_percent1 and battery_percent2)):
        continue
    message = {
        "ready":True,
        "wand1_state": {
            "drawingMode":wand1_drawingMode.is_set(),
            "spell":wand1_spell,
        },
        "wand2_state": {
            "drawingMode":wand2_drawingMode.is_set(),
            "spell":wand2_spell,
        }
    }
    cli.publish(T_U96_STATUS, json.dumps(message), 1, True)
    gameReady.set()
    # Start game when all players ready
    threading.Thread(target=checkPauseLoop, daemon=True).start()
    threading.Thread(target=updateTimeLoop, daemon=True).start()
    # threading.Thread(target=game_loop, daemon=True).start()
    threading.Thread(target=AILoopPlayer1, daemon=True).start()
    threading.Thread(target=AILoopPlayer2, daemon=True).start()
    # modifyBatt(1)
    # modifyBatt(2)
    while True:
        continue


if __name__ == "__main__":
    main()