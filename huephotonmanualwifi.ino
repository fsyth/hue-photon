// Hue Bridge API parameters. These can be obtained manually.
const byte ip[] = { 192, 168, 1, 118 };
const String id = "PPie0zfjcGDOrUinw6RcGXAo0q3bhHyyeioxVPzD";
const int lights[] = { 1, 2, 3 };

// These variables are derived from the Hue parameters specified above
const int lightCount = sizeof(lights) / sizeof(lights[0]);
const String path = "/api/" + id + "/lights/";

// Define analog pins for each of the potentiometers, plus indicator LED
const int huePin = A0;
const int satPin = A1;
const int briPin = A2;
const int ledPin = D7;

// If byte values change by more than the threshold, the lights will be updated
const int threshold = 8;

// Add a delay of a number of milliseconds into the main loop to avoid
// sending too many requests to the Hue Bridge and to save power
const int loopInterval = 500;

// The wifi module should be turned off after a number of loops to save power
const int offAfter = 60e3 / loopInterval;


// Declare values for the three colour parameters, and on/off
// hue: 0-0xFFFF, sat: 0-0xFF, bri: 0-0xFF
int hue, sat, bri = 0;
bool on = false;

// Declare a JSON formatted string to be sent to the Hue Bridge
String json;

// Create a TCPClient that will send on the JSON data
TCPClient client;

// Keep count of the number of loops that have been processed
// If a number of loops pass without change, the wifi module can be turned off
// to conserve battery life
int counter = 0;
bool wifiEnabled = true;


// Sets up pins, Particle variables, and tests connection to Hue Bridge.
// The LED will flash twice to show that the connection was successful.
void setup() {
    // Set potentiometer pins to be inputs
    pinMode(huePin, INPUT);
    pinMode(satPin, INPUT);
    pinMode(briPin, INPUT);

    // Set LED to be an output
    pinMode(ledPin, OUTPUT);

    // Make hue, sat, bri into Particle variables so they are accessible in the cloud
    Particle.variable("hue",  &hue,  INT);
    Particle.variable("sat",  &sat,  INT);
    Particle.variable("bri",  &bri,  INT);
    Particle.variable("on",   &on,   BOOLEAN);
    Particle.variable("json", &json, STRING);

    // Setup the server, and wait until it's ready
    if (client.connect(ip, 80)) {
        // Flash the onboard LED to show connected
        ledFlash(200, 2);
        client.stop();
    }
}


// Reads analogue values for each of the potentiometers and deals with any changes, including:
//   - the brightness potentiometer changing between on or off -> PUT on/off
//   - changes to hue, saturation or brightness -> PUT hue, sat, bri
//   - a long time with no changes -> WiFi off
void loop() {
    // Read the values of the three potentiometers into temporary variables to check for changes.
    // The analogRead function gives 12-bit resolution.
    // The dimmers rotate the wrong way, so subtract from the max 12-bit value to reverse.
    // The Hue Bridge expects 16-, 8- and 8-bit resolution for the three values so bit-shift
    // as required.
    int hue_ = (0xFFF - analogRead(huePin)) << 4;
    int sat_ = (0xFFF - analogRead(satPin)) >> 4;
    int bri_ = (0xFFF - analogRead(briPin)) >> 4;
    bool on_ = bri_ > 0;

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
    if (on_ && changed(hue_, sat_, bri_)) {
        // A change has occurred, so update values
        hue = hue_;
        sat = sat_;
        bri = bri_;

        // Build a JSON string with the values to be sent off
        json = "{\"hue\":" + String(hue, DEC) +
               ",\"sat\":" + String(sat, DEC) +
               ",\"bri\":" + String(bri, DEC) + "}";

        // Send the data to the Hue Bridge
        sendJSON();
    }

    // Turn off the wifi module if nothing has changed in a while
    if (wifiEnabled && ++counter > offAfter) {
        wifiEnabled = false;
        WiFi.off();

        // Flash the LED to show entering power-save mode
        ledFlash(100, 3);
    }

    // No need to run this as fast as possible. To save power, build in a delay.
    delay(loopInterval);
}


// Compares the newly measured values of HSB from the potentiometers against the
// current state and returns true if the values have significantly changed.
bool changed(const int h_, const int s_, const int b_) {
    return abs(b_ - bri) > threshold ||
           abs(s_ - sat) > threshold ||
           abs(h_ - hue) > (threshold << 8);
}


// Sends the string data stored in the json variable to the Hue Bridge.
// It also re-enables the wifi module if it was previously disabled and
// resets the counter for loops without changes occurring.
void sendJSON() {
    // Turn the wifi module on if it was previously disabled
    if (!wifiEnabled) {
        wifiEnabled = true;
        WiFi.on();

        // Wait until the wifi module is ready before proceeding.
        while (!WiFi.ready()) Particle.process();

        // Reset counter
        counter = 0;
    }

    digitalWrite(ledPin, HIGH);
    // Send a put request to each light
    for (int i = 0; i < lightCount; i++) {
        if (client.connect(ip, 80)) {
            client.print("PUT ");
            client.print(path);
            client.print(lights[i]);
            client.println("/state HTTP/1.0");
            client.print("Host: ");
            client.print(ip[0]);
            client.print(".");
            client.print(ip[1]);
            client.print(".");
            client.print(ip[2]);
            client.print(".");
            client.println(ip[3]);
            //client.println("Connection: keep-alive");
            // ^^^ not needed if reconnecting each time
            client.println("User-Agent: Particle-Photon");
            client.println("Content-Type: application/json");
            client.print("Content-Length: ");
            client.println(json.length());
            client.println();
            client.println(json);
            client.println();
        }
    }
    digitalWrite(ledPin, LOW);
}


// Turns the LED on for time t, and then off for time t.
// Units of milliseconds.
void ledFlash(int t) {
    ledFlash(t, 1);
}

// Turns the LED on for time t, and then off for time t.
// This is repeated c times.
void ledFlash(int t, int c) {
    // Flash c times.
    for (int i = 0; i < c; i++) {
        // Turn the LED on for time t before turning off for time t.
        digitalWrite(ledPin, HIGH);
        delay(t);
        digitalWrite(ledPin, LOW);
        delay(t);
    }
}
