// ============================================================
//  Pulse Frequency Classifier — Adafruit Metro M0 Express
//  Arduino IDE
//
//  Determines whether a source is emitting pulses at
//  approximately 312 Hz or 547 Hz.
//
//  Signal: Idle HIGH (3V), Pulse LOW (0V) — inverted
//  Trigger: FALLING edge on pin D2
// ============================================================

// --- Configuration ---
#define PULSE_PIN           2       // Interrupt-capable input pin
#define MEASURE_WINDOW_MS   1000    // Measurement duration (ms)
#define NUM_SAMPLES         5       // Number of measurement rounds to average
#define FREQ_A              312.0f  // Target frequency A (Hz)
#define FREQ_B              547.0f  // Target frequency B (Hz)
#define DECISION_BOUNDARY   ((FREQ_A + FREQ_B) / 2.0f)  // 429.5 Hz

// --- State Machine ---
typedef enum {
  STATE_IDLE,       // Waiting to start
  STATE_COUNTING,   // Actively counting pulses
  STATE_ANALYSE,    // Process the collected samples
  STATE_REPORT      // Print result, then return to idle
} State;

State currentState = STATE_IDLE;

// --- Measurement Globals ---
volatile uint32_t pulseCount  = 0;   // Incremented by ISR
float    samples[NUM_SAMPLES];        // Stores each measured frequency
uint8_t  sampleIndex          = 0;   // Current sample number
uint32_t windowStart          = 0;   // Timestamp of window start

// ============================================================
//  ISR — FALLING edge (inverted pulse: idle HIGH, pulse LOW)
// ============================================================
void onPulseDetected() {
  pulseCount++;
}

// ============================================================
//  Helper — Print a horizontal divider
// ============================================================
void printDivider() {
  Serial.println("  ─────────────────────────────────────────");
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(2000);    // Passive serial init — no while(!Serial) blocking

  pinMode(PULSE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), onPulseDetected, FALLING);

  Serial.println();
  Serial.println("  ╔══════════════════════════════════════════╗");
  Serial.println("  ║     Pulse Frequency Classifier           ║");
  Serial.println("  ║     Metro M0 Express                     ║");
  Serial.println("  ╠══════════════════════════════════════════╣");
  Serial.print  ("  ║  Target A : ");
  Serial.print  (FREQ_A, 1);
  Serial.println(" Hz                         ║");
  Serial.print  ("  ║  Target B : ");
  Serial.print  (FREQ_B, 1);
  Serial.println(" Hz                         ║");
  Serial.print  ("  ║  Boundary : ");
  Serial.print  (DECISION_BOUNDARY, 1);
  Serial.println(" Hz                       ║");
  Serial.print  ("  ║  Samples  : ");
  Serial.print  (NUM_SAMPLES);
  Serial.println(" rounds x ");
  Serial.print  (MEASURE_WINDOW_MS / 1000.0f, 1);
  Serial.println("s            ║");
  Serial.println("  ╚══════════════════════════════════════════╝");
  Serial.println();
  Serial.println("  Send [s] + Enter to begin a measurement.");
  printDivider();
}

// ============================================================
//  Loop — State Machine
// ============================================================
void loop() {

  switch (currentState) {

    // ----------------------------------------------------------
    case STATE_IDLE:
      if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 's' || cmd == 'S') {
          sampleIndex = 0;
          Serial.println();
          Serial.println("  >> Starting measurement...");
          printDivider();
          currentState = STATE_COUNTING;

          // Arm the first measurement window
          noInterrupts();
          pulseCount  = 0;
          interrupts();
          windowStart = millis();
        }
      }
      break;

    // ----------------------------------------------------------
    case STATE_COUNTING:
      if (millis() - windowStart >= MEASURE_WINDOW_MS) {

        // Snapshot pulse count safely
        noInterrupts();
        uint32_t count = pulseCount;
        pulseCount     = 0;           // Reset for next window
        interrupts();

        // Convert count to frequency
        float measuredHz = (float)count / (MEASURE_WINDOW_MS / 1000.0f);
        if((measuredHz<=800)&&(measuredHz>=100)){ //sample is in reasonable range 
          samples[sampleIndex] = measuredHz;
          sampleIndex++;

          Serial.print("  Sample ");
          Serial.print(sampleIndex);
          Serial.print(" / ");
          Serial.print(NUM_SAMPLES);
          Serial.print("  →  ");
          Serial.print(measuredHz, 2);
          Serial.println(" Hz");
          
          if (sampleIndex >= NUM_SAMPLES) {
            currentState = STATE_ANALYSE;   // All samples collected
          } else {
          windowStart = millis();         // Start next window immediately
        }
      }
      else{
        windowStart = millis();
      }
    }
      break;

    // ----------------------------------------------------------
    case STATE_ANALYSE: {
      printDivider();

      // Calculate average frequency across all samples
      float sum = 0.0f;
      float minHz = samples[0];
      float maxHz = samples[0];

      for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += samples[i];
        if (samples[i] < minHz) minHz = samples[i];
        if (samples[i] > maxHz) maxHz = samples[i];
      }

      float avgHz   = sum / NUM_SAMPLES;
      float spreadHz = maxHz - minHz;

      // Calculate how far the average sits from each target (%)
      float errorA = abs(avgHz - FREQ_A) / FREQ_A * 100.0f;
      float errorB = abs(avgHz - FREQ_B) / FREQ_B * 100.0f;

      Serial.println();
      Serial.println("  >> Analysis:");
      Serial.print  ("     Average Frequency : ");
      Serial.print  (avgHz, 2);
      Serial.println(" Hz");
      Serial.print  ("     Sample Spread     : ");
      Serial.print  (spreadHz, 2);
      Serial.println(" Hz (max - min)");
      Serial.print  ("     Error from ");
      Serial.print  (FREQ_A, 0);
      Serial.print  (" Hz  : ");
      Serial.print  (errorA, 2);
      Serial.println(" %");
      Serial.print  ("     Error from ");
      Serial.print  (FREQ_B, 0);
      Serial.print  (" Hz  : ");
      Serial.print  (errorB, 2);
      Serial.println(" %");

      // Store result for STATE_REPORT
      currentState = STATE_REPORT;

      // Pass avgHz forward via a local static
      static float resultHz;
      resultHz = avgHz;

      // Immediately fall through to report
      printDivider();
      Serial.println();

      if (avgHz < DECISION_BOUNDARY) {
        Serial.println("  ╔══════════════════════════════════════════╗");
        Serial.println("  ║  RESULT:  ✓ SOURCE A  —  ~312 Hz        ║");
        Serial.println("  ╚══════════════════════════════════════════╝");
      } else {
        Serial.println("  ╔══════════════════════════════════════════╗");
        Serial.println("  ║  RESULT:  ✓ SOURCE B  —  ~547 Hz        ║");
        Serial.println("  ╚══════════════════════════════════════════╝");
      }

      Serial.println();
      Serial.println("  Send [s] + Enter to run another measurement.");
      printDivider();

      currentState = STATE_IDLE;
      break;
    }
  }
}