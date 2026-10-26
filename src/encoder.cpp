#include "encoder.h"

/*
    encoderSetup
    Initialize the I2C encoders with the custom pins

*/

static i2cEncoderLibV2 RGBEncoder = i2cEncoderLibV2(ENCODER_ADDRESS);

EncoderState g_encoderState = BRIGHTNESS; // default state

static void encoder_onChange(i2cEncoderLibV2 *obj);
static void encoder_onClick(i2cEncoderLibV2 *obj);
static void encoder_doubleClick(i2cEncoderLibV2 *obj);
static void setEncoderState(i2cEncoderLibV2 *obj, EncoderState state);
static void setEncoderColor(i2cEncoderLibV2 *obj, EncoderState state);

void encoderSetup()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Initialize I2C with custom pins

    pinMode(I2C_INT_PIN, INPUT_PULLUP); // INT is open-drain; stay idle if no encoder is attached

    RGBEncoder.reset();

    RGBEncoder.begin(
        i2cEncoderLibV2::INT_DATA |
        i2cEncoderLibV2::WRAP_DISABLE |
        i2cEncoderLibV2::DIRE_RIGHT |
        i2cEncoderLibV2::IPUP_ENABLE |
        i2cEncoderLibV2::RMOD_X1 |
        i2cEncoderLibV2::RGB_ENCODER);
    RGBEncoder.id = 0;                      // do we need this?
    RGBEncoder.writeAntibouncingPeriod(10); // x 0.192ms = ~2ms of debouncing
    RGBEncoder.writeDoublePushPeriod(50);   // Set the double push period to 500ms

    setEncoderState(&RGBEncoder, BRIGHTNESS); // Initial state

    /* Configure the callback events */
    RGBEncoder.onChange = encoder_onChange;
    RGBEncoder.onButtonPush = encoder_onClick;
    RGBEncoder.onButtonDoublePush = encoder_doubleClick;

    /* Enable the I2C Encoder V2 interrupts according to the previus attached callback */
    RGBEncoder.autoconfigInterrupt();

    RGBEncoder.writeCounter((int32_t)ENCODER_DEFAULT);
    RGBEncoder.writeMax((int32_t)ENCODER_MAX);
    RGBEncoder.writeMin((int32_t)ENCODER_MIN);
    RGBEncoder.writeStep((int32_t)ENCODER_STEP);
}

void readEncoder()
{
    if (digitalRead(I2C_INT_PIN) == LOW)
    {
        _taskEncoderAnimation.delay(ENCODER_ANIMATION_IDLE_TIMEOUT);
        RGBEncoder.updateStatus();
    }
}

static void encoder_onChange(i2cEncoderLibV2 *obj)
{
    constexpr const char *SGN = "encoder_onChange()";
    bool increasing = obj->readStatus(i2cEncoderLibV2::RINC);

    // Only the direction matters. Re-centre the counter, otherwise it eventually
    // sticks at ENCODER_MIN/MAX (wrap is disabled) and turning that way stops
    // generating events.
    obj->writeCounter((int32_t)ENCODER_DEFAULT);

    switch (g_encoderState)
    {
    case BRIGHTNESS:
        setEncoderColor(obj, BRIGHTNESS);
        uint8_t step;
        if (g_targetBrightness < 20)
        {
            step = 1; // Fine control at low brightness
        }
        else if (g_targetBrightness < 60)
        {
            step = 3; // Medium steps
        }
        else if (g_targetBrightness < 150)
        {
            step = 5; // Larger steps
        }
        else
        {
            step = 10; // Biggest steps at high brightness
        }

        if (increasing)
        {
            g_targetBrightness = min(MAX_BRIGHTNESS, g_targetBrightness + step);
        }
        else
        {
            g_targetBrightness = max(MIN_BRIGHTNESS, g_targetBrightness - step);
        }
        _taskChangeToBrightness.enableIfNot();
        break;

    case SPEED:
        setEncoderColor(obj, SPEED);
        if (increasing)
        {
            g_animationSpeed = min(MAX_ANIMATION_SPEED, g_animationSpeed + ANIMATION_SPEED_STEP);
        }
        else
        {
            g_animationSpeed = max(MIN_ANIMATION_SPEED, g_animationSpeed - ANIMATION_SPEED_STEP);
        }
        Serial.printf("%s: %s: Animation Speed: %.2f\n", timeToString().c_str(), SGN, g_animationSpeed);
        break;

    case PLAYLIST:
    {
        setEncoderColor(obj, PLAYLIST);
        // A knob that wobbles at a detent can report next and previous in one
        // movement: ignore a step back the other way that follows too soon.
        // (changePattern() prints the pattern it switches to.)
        static uint32_t lastStepMs = 0;
        static int8_t lastDirection = 0;
        int8_t direction = increasing ? 1 : -1;
        uint32_t now = millis();
        if (direction == -lastDirection && now - lastStepMs < PLAYLIST_REVERSE_GUARD_MS)
        {
            Serial.printf("%s: %s: ignored a quick reversal\n", timeToString().c_str(), SGN);
            break;
        }
        lastStepMs = now;
        lastDirection = direction;
        changePattern(direction);
        break;
    }

    default:
        break;
    }
}

// Toggle between BRIGHTNESS and SPEED and PLAYLIST
static void encoder_onClick(i2cEncoderLibV2 *obj)
{
    EncoderState newEncoderstate = BRIGHTNESS;
    if (g_encoderState == BRIGHTNESS)
    {
        newEncoderstate = SPEED;
    }
    else if (g_encoderState == SPEED)
    {
        newEncoderstate = PLAYLIST;
    }
    else if (g_encoderState == PLAYLIST)
    {
        newEncoderstate = BRIGHTNESS;
    }
    setEncoderState(obj, newEncoderstate);
}

static void encoder_doubleClick(i2cEncoderLibV2 *obj)
{
    constexpr const char *SGN = "encoder_doubleClick()";
    if (g_encoderState == PLAYLIST)
    {
        if (_taskChangePattern.isEnabled())
        {
            obj->writeRGBCode(0x0000FF); // Dark Blue
            _taskChangePattern.disable();
            Serial.printf("%s: %s: Disabling change pattern\n", timeToString().c_str(), SGN);
        }
        else
        {
            obj->writeRGBCode(0xFFFFFF); // White
            _taskChangePattern.enableIfNot();
            Serial.printf("%s: %s: Initiating change pattern\n", timeToString().c_str(), SGN);
        }
    }
}

static void setEncoderState(i2cEncoderLibV2 *obj, EncoderState state)
{
    static const char *const names[] = {"BRIGHTNESS", "SPEED", "PLAYLIST"};
    setEncoderColor(obj, state);
    g_encoderState = state;
    Serial.printf("%s: setEncoderState(): EncoderState: %s\n", timeToString().c_str(), names[state]);
}

static void setEncoderColor(i2cEncoderLibV2 *obj, EncoderState state)
{
    // Called on every knob step (to restore the mode colour after the idle
    // animation), so it doesn't print; setEncoderState() reports mode changes.
    switch (state)
    {
    case BRIGHTNESS:
        obj->writeRGBCode(0xFF0000); // Red
        break;
    case SPEED:
        obj->writeRGBCode(0x00FFFF); // Cyan
        break;
    case PLAYLIST:
        obj->writeRGBCode(0xFFFF00); // Yellow
        break;
    }
}
void updateEncoderIdleAnimation()
{
    // Mirror one of the bottom-most LEDs (next to the knob) on the encoder LED
    CRGB rgb = leds[12];
    RGBEncoder.writeRGBCode(((uint32_t)rgb.r << 16) | ((uint32_t)rgb.g << 8) | rgb.b);
}
