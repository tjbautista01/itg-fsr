#include <arduino_psx.h>
#include <Joystick.h>

// current configuration is for a Pro Micro board. Add and/or omit as per your board's specs
Joystick_ Joystick (
    0x03,
    JOYSTICK_TYPE_JOYSTICK,
    10, /* number of buttons usable, limit will be defined by number of usable analog and digital pins*/
    0,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false
);

// Max window size for both of the moving averages classes.
const size_t kWindowSize = 100;

/*===========================================================================*/

// Class containing all relevant information per sensor.
class SensorState {
    public:
        SensorState(unsigned int pin_value, unsigned int offset, unsigned int thresholdValue, 
                    unsigned int psxInput, bool isAnalog) :
            pin_value_(pin_value),
            state_(SensorState::OFF),
            user_threshold_(thresholdValue),
            psxInput_(psxInput),
            isAnalog_(isAnalog),
            offset_(offset) {}

        // Fetches the sensor value and maybe triggers the button press/release.
        void EvaluateSensor(int joystick_num) {
            if (isAnalog_) {
              cur_value_ = analogRead(pin_value_);
              if (cur_value_ >= user_threshold_ + kPaddingWidth &&
                  state_ == SensorState::OFF) {
                  Joystick.setButton(joystick_num, 1);
                  PSX.setButton(psxInput_, true);
                  state_ = SensorState::ON;
              }
              if (cur_value_ < user_threshold_ - kPaddingWidth &&
                  state_ == SensorState::ON) {
                  Joystick.setButton(joystick_num, 0);
                  PSX.setButton(psxInput_, false);
                  state_ = SensorState::OFF;
              }
            } else if (!isAnalog_) {
              if (digitalRead(pin_value_) == LOW &&
                  state_ == SensorState::OFF) {
                  Joystick.setButton(joystick_num, 1);
                  PSX.setButton(psxInput_, true);
                  state_ = SensorState::ON;
              }
              if (digitalRead(pin_value_) == HIGH &&
                  state_ == SensorState::ON) {
                  Joystick.setButton(joystick_num, 0);
                  PSX.setButton(psxInput_, false);
                  state_ = SensorState::OFF;
              }
            }
            
        }

        void UpdateThreshold(unsigned int new_threshold) {
            user_threshold_ = new_threshold;
        }

        int GetCurValue() {
            return cur_value_;
        }

        int GetCurThreshold() {
            return user_threshold_;
        }

        int GetPinValue() {
            return pin_value_;
        }

        int GetIsAnalog() {
            return isAnalog_;
        }

        // Delete default constructor. Pin number MUST be explicitly specified.
        SensorState() = delete;

    private:
        // The pin on the Teensy/Arduino corresponding to this sensor.
        unsigned int pin_value_;
        // The current joystick state of the sensor.
        enum State { OFF, ON };
        State state_;
        // The user defined threshold value to activate/deactivate this sensor at.
        int user_threshold_;
        // PSX input value
        int psxInput_;
        // boolean value to determine if sensor should be read as analog or digital
        bool isAnalog_;
        // One-tailed width size to create a window around user_threshold_ to
        // mitigate fluctuations by noise.
        const int kPaddingWidth = 1;

        int cur_value_;

        int offset_;
};

/*===========================================================================*/

// Defines the sensor collections and sets the pins for them appropriately.
/* When setting sensor states, I've made an implication that the pin order
 * will match how the arrows appear on screen (0: Left, 1: Down, 2: Up, 3: Right).
 * This is only important if you intend on using the web app to control the
 * sensor thresholds, which expects the sensor states in that order.
*/
// parameters: input pin, offset (default to 0), threshold, PSX input assignment, 
//             bool to read pin as analog
// current configuration is for a Pro Micro board. Add and/or omit as per your board's specs
SensorState kSensorStates[] = {
    SensorState(A0, 0, 900, PS_INPUT::PS_LEFT, true),
    SensorState(A1, 0, 900, PS_INPUT::PS_DOWN, true),
    SensorState(A2, 0, 900, PS_INPUT::PS_UP, true),
    SensorState(A3, 0, 900, PS_INPUT::PS_RIGHT, true),
    SensorState(A6, 0, 800, PS_INPUT::PS_CROSS, true),
    SensorState(A7, 0, 800, PS_INPUT::PS_CIRCLE, true),
    SensorState(A8, 0, 800, PS_INPUT::PS_TRIANGLE, true),
    SensorState(A9, 0, 800, PS_INPUT::PS_SQUARE, true),
    SensorState(10, 0, 0, PS_INPUT::PS_SELECT, false),
    SensorState(7, 0, 0, PS_INPUT::PS_START, false)
};
const size_t kNumSensors = sizeof(kSensorStates)/sizeof(SensorState);

/*===========================================================================*/

class SerialProcessor {
    public:
        void Init(unsigned int baud_rate, unsigned int ack_pin) {
            Serial.begin(baud_rate);
            
            // Setup PSX with digital pin input as ACK pin
            PSX.init(ack_pin);
        }

        void CheckAndMaybeProcessData() {
            while (Serial.available() > 0) {
                size_t bytes_read = Serial.readBytesUntil('\n', buffer_, kBufferSize - 1);
                buffer_[bytes_read] = '\0';

                if (strcmp(buffer_, "pressures") == 0) {
                    OutputCurrentPressures();
                    return;
                }
                
                if (strcmp(buffer_, "thresholds") == 0) {
                    OutputCurrentThreshholds();
                    return;
                }

                char* strSens;
                int index;
                for (index = 0; index < kNumSensors; index++) {
                    if (index == 0) {
                        strSens = strtok(buffer_, ",");
                    }
                    else {
                        strSens = strtok(NULL, ",");
                    }
                    
                    unsigned int newThreshhold = strtoul(strSens, nullptr, 10);
                    kSensorStates[index].UpdateThreshold(newThreshhold);
                }
            }
        }

        void OutputCurrentPressures() {
            char message[20];
            // increase as you add more sensors
            sprintf(message, "%d,%d,%d,%d,%d,%d,%d,%d\n", 
                kSensorStates[0].GetCurValue(),
                kSensorStates[1].GetCurValue(),
                kSensorStates[2].GetCurValue(),
                kSensorStates[3].GetCurValue(),
                kSensorStates[4].GetCurValue(),
                kSensorStates[5].GetCurValue(),
                kSensorStates[6].GetCurValue(),
                kSensorStates[7].GetCurValue()
            );            
            Serial.print(message);
        }

        void OutputCurrentThreshholds() {
            char message[20];
            // increase as you add more sensors
            sprintf(message, "%d,%d,%d,%d,%d,%d,%d,%d\n", 
                kSensorStates[0].GetCurThreshold(),
                kSensorStates[1].GetCurThreshold(),
                kSensorStates[2].GetCurThreshold(),
                kSensorStates[3].GetCurThreshold(),
                kSensorStates[4].GetCurThreshold(),
                kSensorStates[5].GetCurThreshold(),
                kSensorStates[6].GetCurThreshold(),
                kSensorStates[7].GetCurThreshold()
            );            
            Serial.print(message);
        }

    private:
        static const size_t kBufferSize = 256;
        char buffer_[kBufferSize];
};

/*===========================================================================*/

SerialProcessor kSerialProcessor;

void setup() {
    kSerialProcessor.Init(9600, 5);
    Joystick.begin();

    // set input mode for digital inputs
    for (size_t i = 0; i < kNumSensors; ++i) {
        if (!kSensorStates[i].GetIsAnalog()) {
          pinMode(kSensorStates[i].GetPinValue(), INPUT_PULLUP);
        }
    }
}

void loop() {
    static unsigned int counter = 0;
    if (counter++ % 10 == 0) {
        kSerialProcessor.CheckAndMaybeProcessData();
    }

    for (size_t i = 0; i < kNumSensors; ++i) {
        kSensorStates[i].EvaluateSensor(i);
    }

    // Send PSX control data to SPI
    PSX.send();
}
