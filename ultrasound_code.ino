// Define the analog pin connected to your circuit's output
const int analogPin = A0; 

// Define the 1.5V threshold (1.5V / 5.0V * 1023 = 306.9)
const int threshold = 465; 

void setup() {
  // Initialize serial communication to the computer at 9600 baud
  Serial.begin(9600);
}

void loop() {
  // Read the raw integer value from the ADC (0 to 1023)
  int sensorValue = analogRead(analogPin);

  // Compare the reading against our 1.5V threshold
  if (sensorValue > threshold) {
    Serial.println("HIGH");
  } else {
    Serial.println("LOW");
  }

  // A brief 10-millisecond delay to prevent flooding the serial port
  delay(10);
}