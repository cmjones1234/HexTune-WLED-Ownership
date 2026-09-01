/******************************************************************************
 *
 * Project      : HexTune
 *
 * Module       : hextune_ownership.cpp
 *
 * Description
 * ---------------------------------------------------------------------------
 * HexTune physical WS2812B ownership Usermod for WLED.
 *
 * Hardware Contract
 * ---------------------------------------------------------------------------
 * D1 GPIO23 -> S3 GPIO8
 *
 * GPIO8 LOW:
 *     D1 owns the physical WS2812B strip.
 *     WLED stops servicing the LED strip and forces it OFF.
 *
 * GPIO8 HIGH:
 *     S3/WLED owns the physical WS2812B strip.
 *     WLED resumes its existing configured LED bus.
 *
 * WLED LED DATA:
 *     GPIO5
 *
 * Important
 * ---------------------------------------------------------------------------
 * This Usermod deliberately does NOT remove WLED buses.
 *
 * Removing buses caused WLED's configured LED bus state to disappear and
 * allowed a zero-bus configuration to be persisted. That produced the
 * observed "0 LEDs / no GPIO after reboot" behavior and made the restored
 * output unreliable.
 *
 * Instead, WLED's existing bus remains configured at all times. When D1 owns
 * the physical strip, the WLED strip service is suspended and the buses are
 * forced OFF. When ownership returns, the strip is resumed and a fresh frame
 * is triggered.
 *
 ******************************************************************************/

#include <new>

#include "wled.h"
#include "bus_manager.h"


class HexTuneOwnership : public Usermod
{
public:

    static constexpr uint8_t OWNER_INPUT_PIN = 8U;

    bool m_initialized = false;
    bool m_lastRemoteOwnership = false;


    void setup() override
    {
        /*
         * GPIO8 is driven by D1 GPIO23.
         *
         * LOW  = D1 owns the strip.
         * HIGH = S3/WLED owns the strip.
         *
         * Pulldown makes D1 ownership the safe boot state.
         */
        pinMode(
            OWNER_INPUT_PIN,
            INPUT_PULLDOWN);

        const bool remoteOwnership =
            digitalRead(OWNER_INPUT_PIN) == HIGH;

        m_lastRemoteOwnership =
            remoteOwnership;

        m_initialized =
            true;

        applyOwnership(
            remoteOwnership);
    }


    void loop() override
    {
        if (!m_initialized)
        {
            return;
        }

        const bool remoteOwnership =
            digitalRead(OWNER_INPUT_PIN) == HIGH;

        if (remoteOwnership ==
            m_lastRemoteOwnership)
        {
            return;
        }

        m_lastRemoteOwnership =
            remoteOwnership;

        applyOwnership(
            remoteOwnership);
    }


private:

    void applyOwnership(
        bool remoteOwnership)
    {
        if (remoteOwnership)
        {
            acquireWLED();
        }
        else
        {
            releaseWLED();
        }
    }


    void releaseWLED()
    {
        /*
         * Stop the effect/service engine first so WLED cannot immediately
         * repaint the strip after it has been handed to the D1.
         */
        strip.suspend();

        /*
         * Force all configured WLED buses OFF.
         *
         * The buses themselves remain allocated and configured, so WLED's
         * persisted LED settings are never destroyed.
         */
        BusManager::off();

#if defined(HT_DEBUG_SERIAL)
        DEBUG_PRINTLN(F("HexTune: WLED strip suspended; D1 owns LEDs."));
#endif
    }


    void acquireWLED()
    {
        /*
         * Resume the same WLED bus configuration that was initialized at
         * startup. No BusConfig reconstruction is necessary.
         */
        strip.resume();

        /*
         * Force the first frame after ownership return.
         */
        strip.trigger();

#if defined(HT_DEBUG_SERIAL)
        DEBUG_PRINTLN(F("HexTune: WLED strip resumed; S3 owns LEDs."));
#endif
    }


public:

    uint16_t getId() override
    {
        return USERMOD_ID_UNSPECIFIED;
    }
};


static HexTuneOwnership hexTuneOwnership;


REGISTER_USERMOD(
    hexTuneOwnership);
