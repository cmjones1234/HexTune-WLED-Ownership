/******************************************************************************
 *
 * Project      : HexTune
 *
 * Module       : hextune_ownership.cpp
 *
 * Purpose
 * ---------------------------------------------------------------------------
 * HexTune physical LED ownership control for WLED.
 *
 * Hardware Contract
 * ---------------------------------------------------------------------------
 * D1 GPIO23 -> S3 GPIO8
 *
 * GPIO8 LOW:
 *     D1 owns the physical WS2812B strip.
 *
 * GPIO8 HIGH:
 *     S3 / WLED owns the physical WS2812B strip.
 *
 * WLED LED DATA:
 *     GPIO5
 *
 * Important
 * ---------------------------------------------------------------------------
 * This Usermod deliberately does NOT manipulate GPIO5 directly.
 *
 * WLED owns and initializes its LED bus normally.  Ownership is handled by
 * suspending/resuming WLED's strip processing rather than destroying or
 * rebuilding the WLED bus.
 *
 ******************************************************************************/

#include <new>

#include "wled.h"
#include "bus_manager.h"


class HexTuneOwnership : public Usermod
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Hardware
    ///////////////////////////////////////////////////////////////////////////

    static constexpr uint8_t OWNER_INPUT_PIN = 8U;


private:

    ///////////////////////////////////////////////////////////////////////////
    // State
    ///////////////////////////////////////////////////////////////////////////

    bool m_initialized = false;
    bool m_remoteOwnership = false;


    ///////////////////////////////////////////////////////////////////////////
    // Apply Ownership
    ///////////////////////////////////////////////////////////////////////////

    void applyOwnership(bool remoteOwnership)
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


    ///////////////////////////////////////////////////////////////////////////
    // Release WLED
    ///////////////////////////////////////////////////////////////////////////

    void releaseWLED()
    {
        /*
         * Stop WLED from servicing/transmitting the LED strip.
         *
         * Do NOT:
         *   - remove the WLED bus
         *   - recreate the WLED bus
         *   - change GPIO5 pinMode
         *
         * WLED retains its normal configured bus.
         */

        strip.suspend();

        /*
         * Make sure any update already in progress has completed before
         * ownership changes.
         */

        while (strip.isUpdating())
        {
            yield();
        }

        /*
         * Turn WLED's logical output off without destroying its bus
         * configuration.
         */

        BusManager::off();

#if defined(HT_DEBUG_SERIAL)

        DEBUG_PRINTLN(
            F("HexTune: WLED suspended; D1 owns LEDs.")
        );

#endif
    }


    ///////////////////////////////////////////////////////////////////////////
    // Acquire WLED
    ///////////////////////////////////////////////////////////////////////////

    void acquireWLED()
    {
        /*
         * WLED's physical bus was never removed, so simply resume the
         * existing WLED LED system.
         */

        strip.resume();

        /*
         * Force WLED to generate/transmit a fresh frame immediately.
         */

        strip.trigger();

        strip.show();

#if defined(HT_DEBUG_SERIAL)

        DEBUG_PRINTLN(
            F("HexTune: WLED resumed; S3 owns LEDs.")
        );

#endif
    }


public:

    ///////////////////////////////////////////////////////////////////////////
    // Setup
    ///////////////////////////////////////////////////////////////////////////

    void setup() override
    {
        /*
         * GPIO8 receives the D1 ownership signal.
         *
         * LOW  = D1 owns LEDs
         * HIGH = S3/WLED owns LEDs
         *
         * Pull-down provides a deterministic LOW state if the D1 signal is
         * disconnected or floating during startup.
         */

        pinMode(
            OWNER_INPUT_PIN,
            INPUT_PULLDOWN
        );


        /*
         * WLED has already initialized its LED bus before Usermod setup().
         * Read the ownership state and apply it once.
         */

        const bool remoteOwnership =
            (digitalRead(OWNER_INPUT_PIN) == HIGH);


        m_remoteOwnership = remoteOwnership;
        m_initialized = true;


        applyOwnership(
            remoteOwnership
        );
    }


    ///////////////////////////////////////////////////////////////////////////
    // Loop
    ///////////////////////////////////////////////////////////////////////////

    void loop() override
    {
        if (!m_initialized)
        {
            return;
        }


        const bool remoteOwnership =
            (digitalRead(OWNER_INPUT_PIN) == HIGH);


        /*
         * Nothing changed.
         */

        if (remoteOwnership == m_remoteOwnership)
        {
            return;
        }


        /*
         * Ownership changed.
         */

        m_remoteOwnership = remoteOwnership;


        applyOwnership(
            remoteOwnership
        );
    }


    ///////////////////////////////////////////////////////////////////////////
    // Usermod ID
    ///////////////////////////////////////////////////////////////////////////

    uint16_t getId() override
    {
        return USERMOD_ID_UNSPECIFIED;
    }
};


///////////////////////////////////////////////////////////////////////////////
// Usermod Instance
///////////////////////////////////////////////////////////////////////////////

static HexTuneOwnership hexTuneOwnership;


///////////////////////////////////////////////////////////////////////////////
// Register Usermod
///////////////////////////////////////////////////////////////////////////////

REGISTER_USERMOD(
    hexTuneOwnership
);
