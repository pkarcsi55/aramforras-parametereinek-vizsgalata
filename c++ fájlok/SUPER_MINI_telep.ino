/*
  ESP32-C3 Super Mini – automatikus impulzusos terhelésmérés
  Kimenet: PWM,Ug_V,Ut_V,Uk_V,I_mA  (CSV, vessző, tizedespont)
  LED: villog keresés alatt, világít mérés alatt, kialszik a végén.
  Indítás: 's' karakter.
  Keresés: PWM=90-től indul, 2-es lépés.
  Mérés: első 10 lépés 2-es, utána 1-es.
*/

#include <Arduino.h>

// ======================== Lábkiosztás ========================
const uint8_t PWM_PIN   = 4;
const uint8_t UK_PIN    = 0;
const uint8_t USONT_PIN = 1;
const uint8_t UGATE_PIN = 3;

const uint8_t LED_PIN = 8;   // beépített LED GPIO8

// ======================== Feszültségosztó szorzók ========================
const float UK_SZORZO    = 1.0;
const float USONT_SZORZO = 1.0;
const float UGATE_SZORZO = 1.0;

// ======================== Mérési paraméterek ========================
const float SONT_OHM      = 10.0;
const float MIN_ARAM_mA   = 3.0;
const float MAX_ARAM_mA   = 130.0;
const uint8_t KERESES_LEPES = 2;
const uint8_t MINTAK_SZAMA = 16;

const unsigned long KERESES_IMPULZUS_MS = 300;
const unsigned long KERESES_SZUNET_MS   = 150;
const unsigned long PIHENO_MS           = 300;
const unsigned long MERESI_IMPULZUS_MS  = 300;

// ======================== PWM beállítások ========================
const uint32_t PWM_FREKVENCIA_HZ = 5000;
const uint8_t PWM_FELBONTAS_BIT  = 8;

struct Meres {
  float uk_V;
  float usont_V;
  float ug_V;
  float aram_mA;
};

// ======================== Függvények ========================

float feszultsegMerese(uint8_t pin, float szorzo) {
  uint32_t osszeg_mV = 0;
  analogReadMilliVolts(pin);
  delayMicroseconds(200);
  for (uint8_t i = 0; i < MINTAK_SZAMA; i++) {
    osszeg_mV += analogReadMilliVolts(pin);
    delayMicroseconds(300);
  }
  float atlag_V = (osszeg_mV / (float)MINTAK_SZAMA) / 1000.0;
  return atlag_V * szorzo;
}

void terhelesKikapcsolasa() {
  ledcWrite(PWM_PIN, 0);
}

Meres meres() {
  Meres adat;
  adat.uk_V    = feszultsegMerese(UK_PIN, UK_SZORZO);
  adat.usont_V = feszultsegMerese(USONT_PIN, USONT_SZORZO);
  adat.ug_V    = feszultsegMerese(UGATE_PIN, UGATE_SZORZO);

  float usont_esés = adat.uk_V - adat.usont_V;
  adat.aram_mA = usont_esés * 1000.0 / SONT_OHM;
  if (adat.aram_mA < 0.0) adat.aram_mA = 0.0;
  return adat;
}

// Keresés: PWM=90-től indul, 2-es lépés
int kezdoPwmKeresese() {
  bool ledState = false;

  // Kezdő PWM = 90
  int startPwm = 120;
  // Ha a 90-nél mért áram már >= 3 mA, akkor 90-ről indulunk (de ezt ritkán várjuk)
  // Ellenőrizzük a 90-es PWM-et
  for (int pwm = startPwm; pwm <= 255; pwm += KERESES_LEPES) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    terhelesKikapcsolasa();
    delay(KERESES_SZUNET_MS);

    ledcWrite(PWM_PIN, pwm);
    delay(KERESES_IMPULZUS_MS);

    Meres adat = meres();
    terhelesKikapcsolasa();

    if (adat.aram_mA >= MIN_ARAM_mA) {
      int kezdo = pwm - KERESES_LEPES;
      if (kezdo < startPwm) kezdo = startPwm;  // biztos, hogy ne legyen 90 alatt
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

  // Mérési ciklus változó lépésközzel
  int step = 2;          // induló lépés
  int stepCount = 0;     // hány lépést tettünk meg
  int pwm = kezdoPwm;

  while (pwm <= 255) {
    terhelesKikapcsolasa();
    delay(PIHENO_MS);

    float ut_V = feszultsegMerese(UK_PIN, UK_SZORZO);

    ledcWrite(PWM_PIN, pwm);
    delay(MERESI_IMPULZUS_MS);

    Meres terhelt = meres();
    terhelesKikapcsolasa();

    // Ha elértük a max áramot, kiírjuk és kilépünk
    if (terhelt.aram_mA >= MAX_ARAM_mA) {
      adatKiirasa(pwm, terhelt.ug_V, ut_V, terhelt.uk_V, terhelt.aram_mA);
      break;
    }

    adatKiirasa(pwm, terhelt.ug_V, ut_V, terhelt.uk_V, terhelt.aram_mA);

    // Lépésköz váltás: az első 10 lépés után átváltunk 1-re
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

  analogReadResolution(12);
  analogSetPinAttenuation(UK_PIN,    ADC_11db);
  analogSetPinAttenuation(USONT_PIN, ADC_11db);
  analogSetPinAttenuation(UGATE_PIN, ADC_11db);

  bool pwmSikeres = ledcAttach(PWM_PIN, PWM_FREKVENCIA_HZ, PWM_FELBONTAS_BIT);
  if (!pwmSikeres) {
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(500);
    }
  }

  ledcWrite(PWM_PIN, 0);

  Serial.println("Send 's' to start measurement.");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);

      automatikusMeres();

      Serial.println("Send 's' for another measurement.");
    }
  }

  digitalWrite(LED_PIN, LOW);
  delay(100);
}