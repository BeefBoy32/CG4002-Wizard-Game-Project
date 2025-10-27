# /home/xilinx/ai_engine.py
from pynq import Overlay, allocate
import numpy as np

# Load bitstream once at import time
_ol = Overlay("/home/xilinx/bitstream/system_wrapper.xsa")
_ol.download()
_dma = _ol.axi_dma_0
_ip  = _ol.cnn_gd_0

# input=60x6 float32, output=7 float32 logits
_IN_SHAPE = (60, 6)
_OUT_DIM  = 7

_inp = allocate(shape=(np.prod(_IN_SHAPE),), dtype=np.float32)
_out = allocate(shape=(_OUT_DIM,), dtype=np.float32)

# Class mapping (keep in sync with your training order)
# 0..6 -> Wave, Circle, Square, Triangle, Infinity, Zigzag, None
_CLASS_TO_LETTER = {0:'W', 1:'C', 2:'S', 3:'T', 4:'I', 5:'Z', 6:'U'}

def class_to_letter(idx: int) -> str:
    return _CLASS_TO_LETTER.get(int(idx), 'U')

def _softmax(x: np.ndarray) -> np.ndarray:
    x = x - np.max(x)
    e = np.exp(x, dtype=np.float32)
    return e / np.sum(e)

def infer(window_60x6: np.ndarray):
    """
    window_60x6: np.ndarray of shape (60,6) float32
                 columns = [yaw, pitch, roll, accelx, accely, accelz]
                 Values must match your training scale (e.g. degrees & m/s^2).
    Returns: (class_index:int, probs:np.ndarray[7], letter:str)
    """
    if window_60x6.shape != _IN_SHAPE:
        raise ValueError(f"expected {_IN_SHAPE}, got {window_60x6.shape}")
    if window_60x6.dtype != np.float32:
        window_60x6 = window_60x6.astype(np.float32, copy=False)

    # Load input buffer
    _inp[:] = window_60x6.flatten()
    _inp.flush(); _out.fill(0); _out.invalidate()

    # Kick DMA + IP
    _dma.recvchannel.transfer(_out)
    _dma.sendchannel.transfer(_inp)
    _ip.register_map.CTRL.AUTO_RESTART = 0
    _ip.register_map.CTRL.AP_START = 1
    _dma.sendchannel.wait()
    _dma.recvchannel.wait()

    _out.invalidate()
    logits = np.array(_out, copy=True)
    probs  = _softmax(logits)
    cls    = int(np.argmax(probs))
    return cls, probs, class_to_letter(cls)
