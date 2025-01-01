# code.py goes in the device's CIRCUITPY root directory on the Adafruit KB2040

import time
import board
import pwmio
import digitalio

# ----------------------------
# FSK & BAUD RATE CONFIG
# ----------------------------
BAUD_RATE = 300
FREQ0 = 1200  # Mark frequency (Hz)
FREQ1 = 2200  # Space frequency (Hz)
interval = 300  # seconds between transmissions

# ----------------------------
# TIMING CONSTANTS
# ----------------------------
# How long to wait after keying the transmitter, before sending FSK
PTT_KEYUP_DELAY = 0.5

# How long to wait after the end sequence, before releasing PTT
PTT_KEYDOWN_DELAY = 0.5

# ----------------------------
# STATION CONFIG
# ----------------------------
CALLSIGN = "ke0sgq"
PREAMBLE = "101010101010"   # 12 alternating bits
END_SEQUENCE = "11111111"   # 8 bits to signify end of transmission

# ----------------------------
# SETUP THE PWM & PTT
# ----------------------------
pwm = pwmio.PWMOut(board.A0, frequency=FREQ0, duty_cycle=32767)

ptt = digitalio.DigitalInOut(board.D9)
ptt.direction = digitalio.Direction.OUTPUT
ptt.value = False  # Start with PTT off

def set_tone(freq):
    """Re-initialize the PWM with the requested frequency."""
    global pwm
    pwm.deinit()
    pwm = pwmio.PWMOut(board.A0, frequency=freq, duty_cycle=32767)

def send_bit(bit):
    """Send a single bit using FSK."""
    set_tone(FREQ1 if bit else FREQ0)
    time.sleep(1.0 / BAUD_RATE)

def send_preamble():
    """Send the preamble bits."""
    for bit in PREAMBLE:
        send_bit(int(bit))

def send_end_sequence():
    """Send a distinct bit pattern to signify transmission end."""
    for bit in END_SEQUENCE:
        send_bit(int(bit))

def send_byte(byte):
    """Send a single byte as 8 bits, MSB first."""
    for i in range(7, -1, -1):
        bit = (ord(byte) >> i) & 1
        send_bit(bit)

def send_string(message):
    """Send a complete string (ASCII)."""
    for char in message:
        send_byte(char)

def transmit_packet(payload):
    """Transmit a complete packet with preamble, payload, callsign, and end sequence."""
    print("Transmitting:", payload)

    # Key the transmitter
    ptt.value = True

    # Wait some time before actually transmitting any FSK tones
    time.sleep(PTT_KEYUP_DELAY)

    # Send the packet
    send_preamble()
    send_string(payload)
    send_string(CALLSIGN)
    send_end_sequence()

    # Wait some extra time after finishing the end sequence, 
    # keeping PTT active but sending no further tones
    time.sleep(PTT_KEYDOWN_DELAY)

    # Finally release PTT
    ptt.value = False

    # (Optional) small extra settle time after unkeying
    time.sleep(0.1)

# ----------------------------
# MAIN LOOP
# ----------------------------
while True:
    transmit_packet("hello world")
    time.sleep(interval)  # Wait N seconds between transmissions
