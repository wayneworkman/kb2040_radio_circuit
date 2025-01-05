# code.py - Place this file in the root directory of your CircuitPython device

import time
import board
import pwmio
import digitalio

# ----------------------------
# PIN CONFIGURATION
# ----------------------------
# KB2040 pin assignments
FSK_OUTPUT_PIN = board.A0    # FSK tone output pin (PWM capable)
PTT_PIN = board.D9           # Push-to-Talk control pin
POWER_PIN = board.D8         # Power control pin

# ----------------------------
# TIMING CONSTANTS
# ----------------------------
POWER_BEFORE_PTT = 4.0       # Seconds to wait after power on before PTT
POWER_AFTER_PTT = 1.0        # Seconds to wait after PTT before power off
CYCLE_TIME = 12.0            # Time between transmissions
PTT_KEYUP_DELAY = 1.0
PTT_KEYDOWN_DELAY = 0.5

# ----------------------------
# FSK & BAUD RATE CONFIG
# ----------------------------
BAUD_RATE = 300
FREQ0 = 1200  # Mark frequency (Hz)
FREQ1 = 2200  # Space frequency (Hz)

# ----------------------------
# STATION CONFIG
# ----------------------------
CALLSIGN = "ke0sgq"
PREAMBLE = "101010101010"   # 12 alternating bits
END_SEQUENCE = "11111111"   # 8 bits to signify end of transmission

# ----------------------------
# SETUP THE PTT & POWER
# ----------------------------
ptt = digitalio.DigitalInOut(PTT_PIN)
ptt.direction = digitalio.Direction.OUTPUT
ptt.value = False  # Start with PTT off

power = digitalio.DigitalInOut(POWER_PIN)
power.direction = digitalio.Direction.OUTPUT
power.value = False  # Start with power off

# We'll create a global reference to PWM but not initialize it right away.
pwm = None

def set_tone(freq):
    """
    Initialize or re-initialize the PWM with the requested frequency.
    This function also enables PWM output when called.
    """
    global pwm

    # Disable any existing PWM output first
    if pwm is not None:
        pwm.deinit()

    # Now enable PWM output at the given frequency
    pwm = pwmio.PWMOut(FSK_OUTPUT_PIN, frequency=freq, duty_cycle=32767)

def send_bit(bit):
    """
    Send a single bit using FSK.
    Mark (1) = FREQ1, Space (0) = FREQ0
    """
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
    """
    Transmit a complete packet with preamble, payload, callsign,
    and end sequence. PWM output is enabled only while sending bits.
    """
    global pwm

    print("Transmitting:", payload)
    
    # Turn on power and wait
    power.value = True
    print("Power on, waiting", POWER_BEFORE_PTT, "seconds")
    time.sleep(POWER_BEFORE_PTT)

    # Key the transmitter
    ptt.value = True

    # Wait some time before actually transmitting any FSK tones
    time.sleep(PTT_KEYUP_DELAY)

    # -- BEGIN FSK TRANSMISSION --
    send_preamble()
    send_string(payload)
    send_string(CALLSIGN)
    send_end_sequence()
    # -- END FSK TRANSMISSION --

    # Disable PWM output after sending the end sequence
    if pwm is not None:
        pwm.deinit()
        pwm = None

    # Wait some extra time keeping PTT active but sending no further tones
    time.sleep(PTT_KEYDOWN_DELAY)

    # Release PTT
    ptt.value = False
    
    # Wait after PTT before turning off power
    print("PTT off, waiting", POWER_AFTER_PTT, "seconds before power off")
    time.sleep(POWER_AFTER_PTT)
    
    # Turn off power
    power.value = False
    print("Power off")

# ----------------------------
# MAIN LOOP
# ----------------------------
while True:
    print("\nStarting transmission cycle")
    transmit_packet("hello world")
    
    # Calculate remaining time in the cycle
    elapsed = POWER_BEFORE_PTT + PTT_KEYUP_DELAY + PTT_KEYDOWN_DELAY + POWER_AFTER_PTT
    remaining = CYCLE_TIME - elapsed
    if remaining > 0:
        print("Waiting", remaining, "seconds until next cycle")
        time.sleep(remaining)

