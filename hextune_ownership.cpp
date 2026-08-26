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
 *     WLED releases its GPIO5 LED bus.
 *
 * GPIO8 HIGH:
 *     S3/WLED owns the physical WS2812B strip.
 *     WLED recreates its configured GPIO5 LED bus.
 *
 * WLED LED DATA:
 *     GPIO5
 *
 ******************************************************************************/

#include "wled.h"
#include "bus_manager.h"


class HexTuneOwnership
    : public Usermod
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Configuration
    ///////////////////////////////////////////////////////////////////////////

    static constexpr uint8_t OWNER_INPUT_PIN =
        8U;

    static constexpr uint8_t WLED_DATA_PIN =
        5U;


    ///////////////////////////////////////////////////////////////////////////
    // State
    ///////////////////////////////////////////////////////////////////////////

    bool m_initialized =
        false;

    bool m_lastRemoteOwnership =
        false;


    ///////////////////////////////////////////////////////////////////////////
    // Setup
    ///////////////////////////////////////////////////////////////////////////

    void setup() override
    {
        /*
         * Internal pulldown provides a deterministic LOCAL/D1 state while
         * the D1 is booting and before GPIO23 has been initialized.
         */
        pinMode(
            OWNER_INPUT_PIN,
            INPUT_PULLDOWN);


        const bool remoteOwnership =
            digitalRead(
                OWNER_INPUT_PIN) ==
            HIGH;


        m_lastRemoteOwnership =
            remoteOwnership;


        m_initialized =
            true;


        /*
         * The desired boot state is LOCAL/D1 whenever GPIO8 is LOW.
         *
         * WLED has already initialized its normal LED bus before Usermod
         * setup() is called, so explicitly release it here when D1 owns
         * the strip.
         */
        applyOwnership(
            remoteOwnership);
    }


    ///////////////////////////////////////////////////////////////////////////
    // Main Loop
    ///////////////////////////////////////////////////////////////////////////

    void loop() override
    {
        if (!m_initialized)
        {
            return;
        }


        const bool remoteOwnership =
            digitalRead(
                OWNER_INPUT_PIN) ==
            HIGH;


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


    ///////////////////////////////////////////////////////////////////////////
    // Apply Ownership
    ///////////////////////////////////////////////////////////////////////////

private:

    void applyOwnership(
        bool remoteOwnership)
    {
        ///////////////////////////////////////////////////////////////////////////
        // D1 / LOCAL
        ///////////////////////////////////////////////////////////////////////////

        if (!remoteOwnership)
        {
            releaseWLED();

            return;
        }


        ///////////////////////////////////////////////////////////////////////////
        // S3 / REMOTE
        ///////////////////////////////////////////////////////////////////////////

        restoreWLED();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Release WLED Physical Output
    ///////////////////////////////////////////////////////////////////////////

    void releaseWLED()
    {
        /*
         * Turn WLED output off before destroying the physical bus.
         */
        BusManager::off();


        /*
         * Destroy the active WLED LED buses.
         *
         * BusDigital::cleanup() releases the WLED ownership of GPIO5 and
         * shuts down the underlying RMT/I2S transport.
         */
        BusManager::removeAll();


        /*
         * Explicitly place GPIO5 in high impedance state.
         *
         * This is the critical handoff point:
         *
         *     GPIO5 = INPUT
         *     D1 GPIO4 = sole WS2812B driver
         */
        pinMode(
            WLED_DATA_PIN,
            INPUT);
    }


    ///////////////////////////////////////////////////////////////////////////
    // Restore WLED Physical Output
    ///////////////////////////////////////////////////////////////////////////

    void restoreWLED()
    {
        /*
         * Recreate the LED buses from the saved WLED configuration.
         */
        const bool aligned =
            strip.checkSegmentAlignment();


        strip.finalizeInit();


        if (aligned)
        {
            strip.makeAutoSegments();
        }
        else
        {
            strip.fixInvalidSegments();
        }


        /*
         * Restore the configured global brightness.
         */
        BusManager::setBrightness(
            strip.getBrightness());


        /*
         * Explicitly restore GPIO5 as an output.
         *
         * finalizeInit() recreates the WLED bus and its driver allocation,
         * but this guarantees the Arduino GPIO mode is correct before the
         * next WLED frame.
         */
        pinMode(
            WLED_DATA_PIN,
            OUTPUT);


        /*
         * Force a frame immediately.
         */
        strip.trigger();
        strip.show();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Identification
    ///////////////////////////////////////////////////////////////////////////

public:

    uint16_t getId()
    {
        return USERMOD_ID_UNSPECIFIED;
    }
};


static HexTuneOwnership
    hexTuneOwnership;


REGISTER_USERMOD(
    hexTuneOwnership);
