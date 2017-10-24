// --- USER VARIABLES --- //

// Optionally change these lines to enable compact wiring layout,
// manual wifi mode, reversed potentiometer voltages, and cloud
// debugging.
#define COMPACT true
#define MANUAL_WIFI false
#define REVERSE_VOLTAGES false
#define CLOUD_ENABLED false

// Hue Bridge API parameters. These can be obtained manually. See README
// for a guide on how to obtain these values
const byte ip[] = { 192, 168, 1, 118 };
const String id = "PPie0zfjcGDOrUinw6RcGXAo0q3bhHyyeioxVPzD";

// The rooms or individual lights to be controlled. Trial and error is
// simplest for finding these values.
// Set room numbers to control all lights in that room.
const int rooms[] = { 1 };
// Additional individual lights that are not already controlled as part
// of a room. There is no need to double up on sending requests.
const int lights[] = {};

// --- END OF USER VARIABLES --- //


// These variables are derived from the Hue parameters specified above
const int lightCount = sizeof(lights) > 0 ? sizeof(lights) / sizeof(lights[0]) : 0;
const int roomCount  = sizeof(rooms)  > 0 ? sizeof(rooms)  / sizeof(rooms[0])  : 0;

// Define analog pins for each of the potentiometers, plus indicator LED
#if COMPACT
const int powPin = A0;
const int huePin = A1;
const int satPin = A2;
const int briPin = A3;
#else
const int huePin = A0;
const int satPin = A1;
const int briPin = A2;
#endif
const int ledPin = D7;

// If 12-bit analog reads values change by more than the threshold,
// the lights will be updated
const int threshold = 1 << 4;

// Add a delay of a number of milliseconds into the main loop to avoid
// sending too many requests to the Hue Bridge and to save power
const int loopInterval = 500;

#if MANUAL_WIFI
// The wifi module should be turned off after a number of loops to save power
const int offAfter = 60e3 / loopInterval;

// Keep count of the number of loops that have been processed.
// If a number of loops pass without change, the wifi module can be turned off
// to conserve battery life.
int counter = 0;
bool wifiEnabled = true;
#endif

// Declare values for the three colour parameters, and on/off
// hue: 0-0xFFFF, sat: 0-0xFF, bri: 0-0xFF
int hue, sat, bri = 0;
bool on = false;

// Declare a JSON formatted string to be sent to the Hue Bridge
String json;

// Create a TCPClient that will send on the JSON data
TCPClient client;


// Sets up pins, Particle variables, and tests connection to Hue Bridge.
// The LED will flash twice to show that the connection was successful.
void setup() {
    // Set potentiometer pins to be inputs
    pinMode(huePin, INPUT);
    pinMode(satPin, INPUT);
    pinMode(briPin, INPUT);

    #if COMPACT
    // Set power pin to be max voltage output
    pinMode(powPin, OUTPUT);
    digitalWrite(powPin, HIGH);
    #endif

    // Set LED to be an output
    pinMode(ledPin, OUTPUT);

    #if CLOUD_ENABLED
    // Make hue, sat, bri into Particle variables so they are accessible in the cloud
    Particle.variable("hue",  &hue,  INT);
    Particle.variable("sat",  &sat,  INT);
    Particle.variable("bri",  &bri,  INT);
    Particle.variable("on",   &on,   BOOLEAN);
    Particle.variable("json", &json, STRING);
    #endif

    // Setup the server, and wait until it's ready
    if (client.connect(ip, 80)) {
        // Flash the onboard LED to show connected
        ledFlash(200);
        ledFlash(200);
        client.stop();
    }
}


// Reads analogue values for each of the potentiometers and deals with any changes, including:
//   - the brightness potentiometer changing between on or off -> PUT on/off
//   - changes to hue, saturation or brightness -> PUT hue, sat, bri
//   - a long time with no changes -> WiFi off
void loop() {
    // Read the values of the three potentiometers into temporary variables to check for changes.
    int hue_ = analogRead(huePin);
    int sat_ = analogRead(satPin);
    int bri_ = analogRead(briPin);

    #if REVERSE_VOLTAGES
    // Turn off at max voltage from the brightness potentiometer
    bool on_ = 0xFFF - bri_ > threshold;
    #else
    // Turn off at min voltage from the brightness potentiometer
    bool on_ = bri_ > threshold;
    #endif

    // Check if the lights are changed between on and off
    if (on_ != on) {
        // A change has occured, so update the value
        on = on_;

        // Generate the JSON string
        json = on ? "{\"on\":true}" : "{\"on\":false}";

        // Send the data to the Hue Bridge
        sendJSON();

        // Wait for the lights to come on before continuing
        delay(loopInterval);
    }

    // Compare the new values to the old values
    if (on_ &&
        abs(bri_ - bri) > threshold ||
        abs(sat_ - sat) > threshold ||
        abs(hue_ - hue) > threshold) {

        // A change has occurred, so update values
        hue = hue_;
        sat = sat_;
        bri = bri_;

        // Build a JSON string with the values to be sent off
        // The analogRead function gives 12-bit resolution.
        // The dimmers rotate the wrong way, so subtract from the max 12-bit value to reverse.
        // The Hue Bridge expects 16-, 8- and 8-bit resolution for the three values so bit-shift
        // as required.
        #if REVERSE_VOLTAGES
        json = "{\"hue\":" + String((0xFFF - hue_) << 4, DEC) +
               ",\"sat\":" + String((0xFFF - sat_) >> 4, DEC) +
               ",\"bri\":" + String((0xFFF - bri_) >> 4, DEC) + "}";
        #else
        json = "{\"hue\":" + String(hue_ << 4, DEC) +
               ",\"sat\":" + String(sat_ >> 4, DEC) +
               ",\"bri\":" + String(bri_ >> 4, DEC) + "}";
        #endif

        // Send the data to the Hue Bridge
        sendJSON();
    }

    #if MANUAL_WIFI
    // Turn off the wifi module if nothing has changed in a while
    if (wifiEnabled && ++counter > offAfter) {
        wifiEnabled = false;
        WiFi.off();

        // Flash the LED to show entering power-save mode
        ledFlash(100);
        ledFlash(100);
        ledFlash(100);
    }
    #endif

    // No need to run this as fast as possible. To save power, build in a delay.
    delay(loopInterval);
}


// Sends the string data stored in the json variable to the Hue Bridge.
// It also re-enables the wifi module if it was previously disabled and
// resets the counter for loops without changes occurring.
void sendJSON() {
    #if MANUAL_WIFI
    // Turn the wifi module on if it was previously disabled
    if (!wifiEnabled) {
        wifiEnabled = true;
        WiFi.on();

        // Wait until the wifi module is ready before proceeding.
        while (!WiFi.ready()) Particle.process();

        // Reset counter
        counter = 0;
    }
    #endif

    // Sending JSON led flash start
    digitalWrite(ledPin, HIGH);

    // Send a PUT request to each room
    for (int i = 0; i < roomCount; i++) {
        if (client.connect(ip, 80)) {
            client.print("PUT /api/");
            client.print(id);
            client.print("/groups/");
            client.print(rooms[i]);
            client.println("/action HTTP/1.0");
            client.print("Content-Length: ");
            client.println(json.length());
            client.println();
            client.println(json);
        }
    }

    // Send a PUT request to each light
    for (int i = 0; i < lightCount; i++) {
        if (client.connect(ip, 80)) {
            client.print("PUT /api/");
            client.print(id);
            client.print("/lights/");
            client.print(lights[i]);
            client.println("/state HTTP/1.0");
            client.print("Content-Length: ");
            client.println(json.length());
            client.println();
            client.println(json);
        }
    }

    // Sending JSON led flash end
    digitalWrite(ledPin, LOW);
}


// Turns the LED on for time t, and then off for time t.
void ledFlash(int t) {
    // Turn the LED on for time t before turning off for time t.
    digitalWrite(ledPin, HIGH);
    delay(t);
    digitalWrite(ledPin, LOW);
    delay(t);
}
