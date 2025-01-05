

## POWER / BARREL PLUG CONNECTIONS

1. **Power Supply**  
   - Has a **female** barrel connector providing 12 V DC.  
   - Red wire = **+12 V** (barrel center pin)  
   - Black wire = **ground** (barrel sleeve)

2. **Device (Radio)**  
   - Has a **male** barrel plug expecting 12 V input.  
   - Red wire = **+12 V** (male center pin)  
   - Black wire = **ground** (male sleeve)

3. **Inline Connection (High-Side P-channel MOSFET Switch, e.g., FQP27P06)**  
   - **Source (S)** → Red wire from **female** barrel (supply +12 V)  
   - **Drain (D)** → Red wire going to **male** barrel (radio +12 V input)  
   - **Gate (G)** → Driven by your NPN transistor driver (if used), or otherwise pulled up to +12 V through a 10 kΩ resistor and pulled low (toward ground) to switch on.  
   - **Black wires** from supply to device are simply **passed straight through** and also connected to the microcontroller ground.

**Common Ground**  
- The negative (black) wire on the supply barrel, the negative (black) wire on the device barrel, and the **KB2040** (RP2040-based board) ground pin **all connect together**. This ensures a single reference voltage for all circuits.

---

## PTT CIRCUIT (2N2222)

1. **KB2040 pin D9** → one end of a **4.7 kΩ** resistor  
2. Other end of that **4.7 kΩ** resistor → **Base** pin of the **2N2222** transistor (PN2222, etc.)  
3. A **470 kΩ** resistor and a **1 nF capacitor** are placed **in parallel** from the **Base** to the **Emitter** of the 2N2222.  
   - This filters high-frequency noise (the 1 nF cap) and provides a pull-down to ground (the 470 kΩ) when D9 is low.  
4. **Collector** of the 2N2222 → **Black wire** that serves as **Mic–** / **PTT** input on the radio.  
5. **Emitter** of the 2N2222 → the **Green wire** (common ground).

**Operation**  
- When the microcontroller drives **pin D9 low**, minimal current flows into the transistor base (4.7 k + 470 k pull-down → transistor is off). The **Mic–** / **PTT** line is not pulled to ground, so transmit is off.  
- When **pin D9** goes high (~3.3 V), current flows through the 4.7 kΩ resistor into the 2N2222 base, saturating the transistor. The collector (Mic– line) is then pulled to ground (via the emitter), activating PTT (push-to-talk).

---

## AUDIO OUTPUT CIRCUIT

1. **KB2040 pin A0** → one end of a **10 kΩ** resistor  
2. Other end of the **10 kΩ** resistor → one end of a **10 nF** capacitor  
3. Other end of the **10 nF** capacitor → **Red wire** (Mic+) on the radio  
4. **Black wire** (Mic–) → the **2N2222 collector** pin (and eventually to ground when the transistor switches)

**Operation**  
- Pin A0 outputs PWM-based tones (FSK signals).  
- The 10 kΩ resistor plus 10 nF capacitor form a simple low-pass / coupling network to feed audio into the radio’s microphone input (Red wire).  
- During PTT, the black wire (Mic–) is grounded through the transistor, completing the mic circuit and enabling transmit audio.

---

## GROUND CONNECTION

1. The **KB2040 ground** pin → **Green wire** = **common ground**  
2. The **Green wire** is connected to the **emitter** of the 2N2222 (PTT circuit) and also to the **negative supply rails** (the black wires on both barrel connectors).  

This single ground reference ensures:

- The 2N2222 transistor can pull Mic– to the correct ground.  
- The audio signal on A0 references the same ground as the radio mic input.  
- The high-side P-channel MOSFET (if you’re using it for power control) also references the same ground for gate drive purposes.

---

### Notes

- The **1 nF capacitor** and **470 kΩ** resistor are in parallel from the 2N2222 **Base** to **Emitter**, filtering RF noise and providing a pull-down so the transistor remains off when D9 is low.  
- During PTT activation, **D9** = HIGH, base current flows through the **4.7 kΩ** resistor, the transistor conducts, and the radio’s **Mic–** line is grounded.  
- The **audio output** from **pin A0** is sent through a **10 kΩ** resistor and **10 nF** capacitor to provide some filtering and AC coupling into the radio’s **Mic+** line.  
- **All black wires** (barrel supply negative, radio negative, KB2040 ground) must join together. This applies even if you have a high-side P-channel MOSFET for switching +12 V—its **source** would be at 12 V, and the negative side is still the same common ground across the entire circuit.

By following these steps and ensuring all grounds (KB2040, supply negative, radio negative) are unified, you’ll have reliable PTT switching, audio output, and optional high-side power control without ground-float issues.