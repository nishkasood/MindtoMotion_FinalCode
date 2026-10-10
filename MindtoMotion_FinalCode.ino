#include <HijelHID_BLEMouse.h>
#include <math.h>

// ╔══════════════════════════════════════════╗
// ║        EMG BLE Mouse  —  ESP32           ║
// ╚══════════════════════════════════════════╝

// ── Hardware ───────────────────────────────
HijelBLEMouse mouse("EMG Mouse", "MakersCode");
constexpr int EMG_PIN = 34;

// ── Sampling ───────────────────────────────
constexpr unsigned long LOOP_MS = 10;   // 100 Hz

// ── Thresholds (based on your measured peaks) ──
constexpr float WEAK_THRESHOLD   = 550.0f;
constexpr float STRONG_THRESHOLD = 1050.0f;
constexpr float RELAX_THRESHOLD  = 300.0f;

// ── Timing ───────────────────────────────
constexpr unsigned long CONFIRM_MS  = 100;
constexpr unsigned long COOLDOWN_MS = 150;

// ── Filter ───────────────────────────────
constexpr float EMA_ALPHA = 0.30f;

// ── Debug ────────────────────────────────
constexpr bool SERIAL_STREAM = false;

// ═══════════════════════════════════════════

enum class State
{
    IDLE,
    DETECTING,
    COOLDOWN
};

State state = State::IDLE;

float baseline    = 0.0f;
float smoothedEMG = 0.0f;
float peakEMG     = 0.0f;

unsigned long flexStart   = 0;
unsigned long cooldownEnd = 0;

// ── Baseline Calibration ──────────────────
void calibrate()
{
    Serial.println("Calibrating — keep arm relaxed...");

    constexpr int N = 1000;
    long sum = 0;

    for (int i = 0; i < N; i++)
    {
        sum += analogRead(EMG_PIN);
        delay(1);
    }

    baseline = sum / (float)N;

    Serial.print("Baseline: ");
    Serial.println(baseline, 1);
}

// ── Click Helper ──────────────────────────
void sendClick(MouseButton btn)
{
    if (!mouse.isPaired())
    {
        Serial.println("Not paired — click skipped.");
        return;
    }

    mouse.click(btn);
}

// ── Setup ─────────────────────────────────
void setup()
{
    Serial.begin(115200);

    calibrate();

    mouse.setBatteryLevel(100);
    mouse.begin();

    Serial.println();
    Serial.println("BLE Mouse Ready");
    Serial.println("Pair with: EMG Mouse");
    Serial.println();

    Serial.print("Weak Threshold: ");
    Serial.println(WEAK_THRESHOLD);

    Serial.print("Strong Threshold: ");
    Serial.println(STRONG_THRESHOLD);

    Serial.print("Relax Threshold: ");
    Serial.println(RELAX_THRESHOLD);
}

// ── Loop ──────────────────────────────────
void loop()
{
    static unsigned long lastRun = 0;
    unsigned long now = millis();

    if (now - lastRun < LOOP_MS)
        return;

    lastRun = now;

    // ── Sample & Filter ───────────────────
    float raw = analogRead(EMG_PIN);

    float emg = fabsf(raw - baseline);

    smoothedEMG =
        EMA_ALPHA * emg +
        (1.0f - EMA_ALPHA) * smoothedEMG;

    if (SERIAL_STREAM)
    {
        Serial.println(smoothedEMG, 1);
    }

    // ── State Machine ─────────────────────
    switch (state)
    {
        case State::IDLE:

            if (smoothedEMG >= WEAK_THRESHOLD)
            {
                state = State::DETECTING;
                flexStart = now;
                peakEMG = smoothedEMG;
            }

            break;

        case State::DETECTING:

            // False trigger
            if (smoothedEMG < RELAX_THRESHOLD)
            {
                state = State::IDLE;
                break;
            }

            // Track strongest point
            if (smoothedEMG > peakEMG)
            {
                peakEMG = smoothedEMG;
            }

            // Confirm flex duration
            if (now - flexStart >= CONFIRM_MS)
            {
                if (peakEMG >= STRONG_THRESHOLD)
                {
                    Serial.println("STRONG FLEX -> RIGHT CLICK");
                    sendClick(MouseButton::Right);
                }
                else
                {
                    Serial.println("WEAK FLEX -> LEFT CLICK");
                    sendClick(MouseButton::Left);
                }

                state = State::COOLDOWN;
                cooldownEnd = now + COOLDOWN_MS;
            }

            break;

        case State::COOLDOWN:

            if (smoothedEMG < RELAX_THRESHOLD &&
                now >= cooldownEnd)
            {
                Serial.println("Ready.");
                state = State::IDLE;
            }

            break;
    }
}