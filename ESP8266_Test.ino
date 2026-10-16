// TEST: ESP8266 + BTS7960 + encoder. Tanpa RS485, tanpa PID.
// Buka Serial Monitor 115200, kirim satu huruf per perintah:
//   f = maju     b = mundur     s = stop
//   + = PWM naik 50     - = PWM turun 50     z = nol-kan encoder
// Tidak ada perintah selama 5 detik: motor berhenti sendiri.
//
// Cara cek:
//   1. Kirim f. Hitungan encoder harus naik. Kalau turun, tukar kabel ENC_A dan ENC_B.
//   2. Kirim z, putar roda satu putaran penuh dengan tangan. Angka enc = COUNTS_PER_REV.
//   3. Kolom adc naik saat beban naik. Isi ADC_OFFSET dengan nilai adc saat motor diam.
//
// Wiring sama dengan program utama:
//   BTS7960 RPWM -> GPIO5 (D1), LPWM -> GPIO4 (D2), R_EN dan L_EN -> 3.3V
//   BTS7960 R_IS + L_IS digabung -> resistor ke GND -> A0
//   Encoder A -> GPIO12 (D6), B -> GPIO14 (D5)
#include <Arduino.h>

const int RPWM = 5, LPWM = 4, ENC_A = 12, ENC_B = 14;
const int ADC_OFFSET = 0;
const uint32_t TIMEOUT_MS = 5000;

volatile long enc = 0;
void IRAM_ATTR isrA() {
  if (digitalRead(ENC_A) == digitalRead(ENC_B)) enc++;
  else enc--;
}

int pwmLevel = 300;  // 0..1023
int dir = 0;         // 1 maju, -1 mundur, 0 stop
uint32_t lastCmd = 0;

void drive() {
  if (dir > 0) {
    analogWrite(LPWM, 0);
    analogWrite(RPWM, pwmLevel);
  } else if (dir < 0) {
    analogWrite(RPWM, 0);
    analogWrite(LPWM, pwmLevel);
  } else {
    analogWrite(RPWM, 0);
    analogWrite(LPWM, 0);
  }
}

void setup() {
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  digitalWrite(RPWM, LOW);
  digitalWrite(LPWM, LOW);

  Serial.begin(115200);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  analogWriteRange(1023);
  analogWriteFreq(10000);
  attachInterrupt(digitalPinToInterrupt(ENC_A), isrA, CHANGE);
  Serial.println("Siap. f/b/s, +/-, z");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'f') dir = 1;
    else if (c == 'b') dir = -1;
    else if (c == 's') dir = 0;
    else if (c == '+') pwmLevel = min(pwmLevel + 50, 1023);
    else if (c == '-') pwmLevel = max(pwmLevel - 50, 0);
    else if (c == 'z') {
      noInterrupts();
      enc = 0;
      interrupts();
    } else continue;
    lastCmd = millis();
    drive();
  }

  if (dir != 0 && millis() - lastCmd > TIMEOUT_MS) {
    dir = 0;
    drive();
    Serial.println("Timeout, motor berhenti");
  }

  static uint32_t lastPrint = 0;
  static long lastEnc = 0;
  const uint32_t DT = 200;  // ms
  if (millis() - lastPrint >= DT) {
    lastPrint = millis();
    noInterrupts();
    long e = enc;
    interrupts();
    float cps = (e - lastEnc) * (1000.0f / DT);  // hitungan per detik
    lastEnc = e;
    int adc = analogRead(A0) - ADC_OFFSET;
    Serial.printf("dir=%d pwm=%d enc=%ld cps=%.0f adc=%d\n", dir, pwmLevel, e, cps, adc);
  }
}
