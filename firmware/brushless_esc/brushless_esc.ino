#include <ESP32Servo.h>

// ---------- Pines ----------
const int PIN_ESC    = 18;   // señal al ESC (cable blanco/naranja)
const int PIN_POT    = 34;   // potenciómetro (pata central)
const int PIN_SWITCH = 27;   // interruptor de dirección
const int PIN_RELE1  = 26;   // módulo relé 1
const int PIN_RELE2  = 25;   // módulo relé 2

// ---------- Configuración ----------
const bool RELE_ACTIVO_BAJO = false; // si los relés funcionan al revés, cambiar a true
const int  PULSO_MIN = 1000;         // µs = 0 %
const int  PULSO_MAX = 2000;         // µs = 100 %
const int  VEL_MAX   = 100;          // límite de velocidad en % (bájalo a 50 para probar)
const bool CALIBRAR  = false;        // true solo para calibrar el ESC

Servo esc;
int  velActual = 0;        // velocidad enviada (%)
bool sentidoActual;        // false = sentido 1, true = sentido 2

// --- Activa o desactiva los dos relés ---
void setRele(bool invertido) {
  bool nivel = RELE_ACTIVO_BAJO ? !invertido : invertido;
  digitalWrite(PIN_RELE1, nivel ? HIGH : LOW);
  digitalWrite(PIN_RELE2, nivel ? HIGH : LOW);
}

// --- Lee el potenciómetro y lo convierte a 0–VEL_MAX % ---
int leerPot() {
  long suma = 0;
  for (int i = 0; i < 16; i++) suma += analogRead(PIN_POT);
  int pct = map(suma / 16, 0, 4095, 0, 100);
  if (pct < 3)  pct = 0;
  if (pct > 97) pct = 100;
  return map(pct, 0, 100, 0, VEL_MAX);
}

// --- Envía la velocidad (%) al ESC ---
void enviar(int pct) {
  esc.writeMicroseconds(map(pct, 0, 100, PULSO_MIN, PULSO_MAX));
}

// --- Lee el interruptor con antirrebote ---
bool leerSwitch() {
  static bool estable = digitalRead(PIN_SWITCH) == LOW;
  static bool ultimo = estable;
  static unsigned long t = 0;
  bool lectura = digitalRead(PIN_SWITCH) == LOW;
  if (lectura != ultimo) { ultimo = lectura; t = millis(); }
  if (millis() - t > 50) estable = lectura;
  return estable;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  pinMode(PIN_RELE1, OUTPUT);
  pinMode(PIN_RELE2, OUTPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_POT, ADC_11db);

  // Pone los relés según la posición inicial del interruptor
  sentidoActual = digitalRead(PIN_SWITCH) == LOW;
  setRele(sentidoActual);

  esc.setPeriodHertz(50);
  esc.attach(PIN_ESC, PULSO_MIN, PULSO_MAX);

  if (CALIBRAR) {
    Serial.println("CALIBRACION: conecte la alimentacion del ESC AHORA (8 s)...");
    esc.writeMicroseconds(PULSO_MAX);
    delay(8000);
    esc.writeMicroseconds(PULSO_MIN);
    Serial.println("Minimo enviado. Espere los pitidos.");
    delay(5000);
  }

  // Armado del ESC
  enviar(0);
  Serial.println("Armando ESC... conecte la alimentacion del ESC.");
  delay(4000);

  // Seguridad: no arranca si el potenciómetro no está en 0
  while (leerPot() > 0) {
    Serial.println("Lleve el potenciometro a 0 para iniciar.");
    delay(500);
  }
  Serial.println("Listo. Gire el potenciometro.");
}

void loop() {
  // --- Cambio de dirección seguro ---
  bool sentidoPedido = leerSwitch();
  if (sentidoPedido != sentidoActual) {
    Serial.println("Cambio de sentido: frenando...");
    while (velActual > 0) { velActual--; enviar(velActual); delay(10); }
    delay(1500);                 // espera a que el motor se detenga
    sentidoActual = sentidoPedido;
    setRele(sentidoActual);      // intercambia las fases
    delay(300);
    Serial.println(sentidoActual ? "Sentido 2" : "Sentido 1");
  }

  // --- Velocidad con rampa suave ---
  int objetivo = leerPot();
  if (velActual < objetivo) velActual++;
  else if (velActual > objetivo) velActual--;
  enviar(velActual);

  // --- Monitor serial ---
  static unsigned long tPrint = 0;
  if (millis() - tPrint > 250) {
    tPrint = millis();
    Serial.printf("Velocidad: %3d %%  |  Pulso: %d us  |  Sentido: %d\n",
                  velActual, map(velActual, 0, 100, PULSO_MIN, PULSO_MAX),
                  sentidoActual ? 2 : 1);
  }
  delay(10);
}