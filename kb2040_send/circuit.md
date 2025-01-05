

## PTT CIRCUIT
1. From KB2040 pin D9 connect to one end of a 4.7kΩ resistor  
2. The other end of the 4.7kΩ resistor connects to the Base pin of the 2N2222 transistor  
3. A 470kΩ resistor connects between the Base pin and the Emitter pin of the 2N2222 transistor  
4. A 1nF capacitor also connects between the Base pin and the Emitter pin of the 2N2222 transistor  
5. The Collector pin of the 2N2222 transistor connects to the Black wire (Mic-, PTT)  
6. The Emitter pin of the 2N2222 transistor connects to the Green wire (common ground)

---

## AUDIO OUTPUT CIRCUIT
1. From KB2040 pin A0 connect to one end of a 10kΩ resistor  
2. The other end of the 10kΩ resistor connects to one end of a 10nF capacitor  
3. The other end of the 10nF capacitor connects to the Red wire (Mic+)  
4. Black wire (Mic-) is connected to the 2N2222 transistor Collector pin and is grounded through the transistor during PTT

---

## GROUND CONNECTION
1. The KB2040 ground pin connects to the Green wire, which serves as the common ground  
2. The Green wire is the reference for the transistor Emitter, radio ground, and circuit ground  

---

### Notes
- The 1nF capacitor and 470kΩ resistor are placed **in parallel** from transistor Base to transistor Emitter.  
- When the microcontroller drives pin D9 low, the 4.7kΩ resistor and 470kΩ pull-down keep the transistor fully off, while the 1nF capacitor filters out stray RF that might otherwise couple into the base.  
- During PTT activation, pin D9 goes high, current flows through the 4.7kΩ resistor to the transistor base, and the transistor grounds the Mic- line (Black wire), engaging the radio’s transmit.