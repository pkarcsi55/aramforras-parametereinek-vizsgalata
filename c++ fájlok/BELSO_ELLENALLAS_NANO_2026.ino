/*
  Arduino Nano – automatikus impulzusos terhelésmeres
  Kimenet: PWM,Ug_V,Ut_V,Uk_V,I_mA  (CSV, vesszo, tizedespont)
  LED: villog kereses alatt, vilagit meres alatt, kialszik a vegen.
  Inditas: 's' karakter (sortoreseket figyelmen kivul hagyja).
*/

// ======================== Labkiosztas (Nano) ========================
const byte PWM_PIN   = 3;
const byte USONT_PIN = A0;
const byte UT_PIN    = A1;
const byte UGATE_PIN = A2;

const byte LED_PIN = LED_BUILTIN;

// ======================== ADC referencia ========================
const float ADC_REFERENCIA_V = 4.69;// kb. 5 V -> megmérendő, ha fontos

// ======================== Meresi parameterek ========================
const float SONT_OHM      = 10.0;
const float MIN_ARAM_mA   = 1.0;
const float MAX_ARAM_mA   = 230.0;
const uint8_t KERESES_LEPES = 2;
const uint8_t MINTAK_SZAMA = 16;

const unsigned long KERESES_IMPULZUS_MS = 300;
const unsigned long KERESES_SZUNET_MS   = 150;
const unsigned long PIHENO_MS           = 300;
const unsigned long MERESI_IMPULZUS_MS  = 300;

// ======================== Struktura ========================
struct Meres {
  float uk_V;
  float usont_V;
  float ug_V;
  float aram_mA;
};

// ======================== Fuggvenyek ========================

float feszultsegMerese(byte pin) {
  unsigned long osszeg = 0;
  analogRead(pin);
  delayMicroseconds(200);
  for (byte i = 0; i < MINTAK_SZAMA; i++) {
    osszeg += analogRead(pin);
    delayMicroseconds(300);
  }
  float atlag = osszeg / (float)MINTAK_SZAMA;
  return atlag * ADC_REFERENCIA_V / 1023.0;
}

void terhelesKikapcsolasa() {
  analogWrite(PWM_PIN, 0);
}

Meres meres() {
  Meres adat;
  adat.uk_V    = feszultsegMerese(UT_PIN);
  adat.usont_V = feszultsegMerese(USONT_PIN);
  adat.ug_V    = feszultsegMerese(UGATE_PIN);

  float usont_es = adat.uk_V - adat.usont_V;
  adat.aram_mA = usont_es * 1000.0 / SONT_OHM;
  if (adat.aram_mA < 0.0) adat.aram_mA = 0.0;
  return adat;
}

int kezdoPwmKeresese() {
  bool ledState = false;
  int startPwm = 90;

  for (int pwm = startPwm; pwm <= 255; pwm += KERESES_LEPES) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    terhelesKikapcsolasa();
    delay(KERESES_SZUNET_MS);

    analogWrite(PWM_PIN, pwm);
    delay(KERESES_IMPULZUS_MS);

    Meres adat = meres();
    terhelesKikapcsolasa();

    if (adat.aram_mA >= MIN_ARAM_mA) {
  // Eredeti: int kezdo = pwm - KERESES_LEPES;
  int kezdo = pwm;   // közvetlenül a talált PWM-ről indul
  if (kezdo < startPwm) kezdo = startPwm;
  digitalWrite(LED_PIN, HIGH);
  return kezdo;
}
  }

  digitalWrite(LED_PIN, LOW);
  return -1;
}

void adatKiirasa(int pwm, float ug_V, float ut_V, float uk_V, float aram_mA) {
  Serial.print(pwm);
  Serial.print(',');
  Serial.print(ug_V, 3);
  Serial.print(',');
  Serial.print(ut_V, 3);
  Serial.print(',');
  Serial.print(uk_V, 3);
  Serial.print(',');
  Serial.println(aram_mA, 2);
}

void automatikusMeres() {
  terhelesKikapcsolasa();
  delay(300);

  int kezdoPwm = kezdoPwmKeresese();

  Serial.println("PWM,Ug_V,Ut_V,Uk_V,I_mA");

  if (kezdoPwm < 0) {
    digitalWrite(LED_PIN, LOW);
    return;
  }

  int step = 2;
  int stepCount = 0;
  int pwm = kezdoPwm;

  while (pwm <= 255) {
    terhelesKikapcsolasa();
    delay(PIHENO_MS);

    float ut_V = feszultsegMerese(UT_PIN);

    analogWrite(PWM_PIN, pwm);
    delay(MERESI_IMPULZUS_MS);

    Meres terhelt = meres();
    terhelesKikapcsolasa();

    if (terhelt.aram_mA >= MAX_ARAM_mA) {
      adatKiirasa(pwm, terhelt.ug_V, ut_V, terhelt.uk_V, terhelt.aram_mA);
      break;
    }

    adatKiirasa(pwm, terhelt.ug_V, ut_V, terhelt.uk_V, terhelt.aram_mA);

    stepCount++;
    if (stepCount >= 10) {
      step = 1;
    }

    pwm += step;
  }

  terhelesKikapcsolasa();
  digitalWrite(LED_PIN, LOW);
}

// ======================== Setup & Loop ========================

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(PWM_PIN, OUTPUT);

  terhelesKikapcsolasa();

  Serial.println("Send 's' to start measurement.");
}

void loop() {
  // Minden bejovo karaktert feldolgozunk, a sortoreseket figyelmen kivul hagyjuk
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);

      automatikusMeres();

      Serial.println("Send 's' for another measurement.");
    }
    // Minden mas karaktert eldobunk (space, newline, stb.)
  }

  digitalWrite(LED_PIN, LOW);
  delay(100);
}