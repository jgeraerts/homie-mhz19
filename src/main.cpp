#include <Arduino.h>
#include <MHZ19.h>
#include <Homie.h>
#include <stdint.h>

const int SEND_INTERVAL = 30000;

HardwareSerial  hwserial1(1);
MHZ19           mhz;
int             mhz_co2_init     = 410;  // magic value reported during init
const int       pin_sensor_rx    = 27;
const int       pin_sensor_tx    = 26;
int          lastSent=0;

HomieNode co2Node("co2", "CO2 Concentration", "mh-z19");

bool calibrate_handler(const HomieRange& range, const String& value) {
  Serial << "calibrating";
  mhz.calibrate();
  return false;
}

void mhz_setup() {
    mhz.begin(hwserial1);
    // mhz.setFilter(true, true);  Library filter doesn't handle 0436
    //mhz.autoCalibration(true);
    char v[5] = {};
    mhz.getVersion(v);
    v[4] = '\0';

    if (strcmp("0436", v) == 0) mhz_co2_init = 436;
}

int mhz_get_co2() {
    int co2       = mhz.getCO2();
    int unclamped = mhz.getCO2(false);

    if (mhz.errorCode != RESULT_OK) {
        delay(500);
        mhz_setup();
        return -1;
    }

    // reimplement filter from library, but also checking for 436 because our
    // sensors (firmware 0436, coincidence?) return that instead of 410...
    if (unclamped == mhz_co2_init && co2 - unclamped >= 10) return 0;

    // No known sensors support >10k PPM (library filter tests for >32767)
    if (co2 > 10000 || unclamped > 10000) return 0;

    return co2;
}

void loopHandler() {
  if (millis() - lastSent > SEND_INTERVAL) {
    int co2Level = mhz_get_co2();
    co2Node.setProperty("concentration").send(String(co2Level));
    lastSent = millis();
  }
}

void setup() {
  lastSent = millis();
  Serial.begin(115200);
  Serial << endl << endl;
  Homie_setFirmware("mh-z19", "1.0.0");
  Homie.setLoopFunction(loopHandler);


  co2Node
    .advertise("concentration")
    .setUnit("ppm")
    .setDatatype("integer")
    .setRetained(false);

  co2Node
    .advertise("calibrate")
    .settable(calibrate_handler);
  
  Homie.setup();
  hwserial1.begin(9600, SERIAL_8N1, pin_sensor_rx, pin_sensor_tx);
  mhz_setup();
}


void loop() {
  Homie.loop();
}
