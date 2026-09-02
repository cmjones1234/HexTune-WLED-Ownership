/******************************************************************************
 *
 * Project      : HexTune
 *
 * Module       : hextune_ownership.cpp
 *
 * Revision     : R0003
 *
 * Description
 * ---------------------------------------------------------------------------
 * HexTune physical WS2812B ownership Usermod for WLED.
 *
 * Hardware Contract
 * ---------------------------------------------------------------------------
 *
 *     D1 GPIO23 -> S3 GPIO8
 *
 *     GPIO8 LOW:
 *         D1 owns the physical WS2812B strip.
 *         S3/WLED releases GPIO5 electrically.
 *
 *     GPIO8 HIGH:
 *         S3/WLED owns the physical WS2812B strip.
 *         S3/WLED drives GPIO5.
 *
 *     WLED LED DATA:
 *         GPIO5
 *
 * Ownership Model
 * ---------------------------------------------------------------------------
 *
 *     The WLED LED bus is NEVER removed or reconstructed.
 *
 *     LOCAL / D1:
 *         strip.suspend()
 *         BusManager::off()
 *         GPIO5 = INPUT / HIGH-Z
 *
 *     REMOTE / S3:
 *         GPIO5 = OUTPUT
 *         strip.resume()
 *         strip.trigger()
 *
 * This preserves WLED's configured LED bus and prevents the previous
 * zero-LED / missing-GPIO configuration problem.
 *
 ******************************************************************************/

#include <new>

#include <Arduino.h>

#include "wled.h"
#include "bus_manager.h"


class HexTuneOwnership : public Usermod
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Hardware
    ///////////////////////////////////////////////////////////////////////////

    static constexpr uint8_t OWNER_INPUT_PIN =
        8U;

    static constexpr uint8_t WLED_DATA_PIN =
        5U;


    ///////////////////////////////////////////////////////////////////////////
    // State
    ///////////////////////////////////////////////////////////////////////////

    bool
        m_initialized =
            false;

    bool
        m_lastRemoteOwnership =
            false;


    ///////////////////////////////////////////////////////////////////////////
    // Setup
    ///////////////////////////////////////////////////////////////////////////

    void setup() override
    {
        ///////////////////////////////////////////////////////////////////////////
        // Ownership Input
        ///////////////////////////////////////////////////////////////////////////

        pinMode(
            OWNER_INPUT_PIN,
            INPUT_PULLDOWN);


        ///////////////////////////////////////////////////////////////////////////
        // Read Initial Ownership
        ///////////////////////////////////////////////////////////////////////////

        const bool remoteOwnership =
            digitalRead(
                OWNER_INPUT_PIN) == HIGH;


        m_lastRemoteOwnership =
            remoteOwnership;

        m_initialized =
            true;


        ///////////////////////////////////////////////////////////////////////////
        // Apply Initial Ownership
        ///////////////////////////////////////////////////////////////////////////

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


        ///////////////////////////////////////////////////////////////////////////
        // Read Ownership Signal
        ///////////////////////////////////////////////////////////////////////////

        const bool remoteOwnership =
            digitalRead(
                OWNER_INPUT_PIN) == HIGH;


        ///////////////////////////////////////////////////////////////////////////
        // Ownership Change
        ///////////////////////////////////////////////////////////////////////////

        if (remoteOwnership !=
            m_lastRemoteOwnership)
        {
            m_lastRemoteOwnership =
                remoteOwnership;


            applyOwnership(
                remoteOwnership);


            return;
        }


        ///////////////////////////////////////////////////////////////////////////
        // Maintain HIGH-Z While D1 Owns the Strip
        ///////////////////////////////////////////////////////////////////////////

        /*
         * WLED can reinitialize its LED bus after certain configuration
         * operations. When D1 owns the physical strip, GPIO5 must remain
         * electrically released.
         *
         * Reassert INPUT only while local ownership is active.
         */
        if (!remoteOwnership)
        {
            pinMode(
                WLED_DATA_PIN,
                INPUT);
        }
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // Apply Ownership
    ///////////////////////////////////////////////////////////////////////////

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


    ///////////////////////////////////////////////////////////////////////////
    // Release WLED / D1 Owns Strip
    ///////////////////////////////////////////////////////////////////////////

    void releaseWLED()
    {
        ///////////////////////////////////////////////////////////////////////////
        // Stop WLED Strip Service
        ///////////////////////////////////////////////////////////////////////////

        strip.suspend();


        ///////////////////////////////////////////////////////////////////////////
        // Force WLED Bus State OFF
        ///////////////////////////////////////////////////////////////////////////

        /*
         * This does NOT remove the configured bus.
         */
        BusManager::off();


        ///////////////////////////////////////////////////////////////////////////
        // Electrically Release GPIO5
        ///////////////////////////////////////////////////////////////////////////

        /*
         * This is the critical S3-side ownership change.
         *
         * WLED's RMT/I2S bus remains allocated and configured, but the ESP32
         * GPIO output driver is disabled so it cannot electrically contend
         * with D1 GPIO4.
         */
        pinMode(
            WLED_DATA_PIN,
            INPUT);


#if defined(HT_DEBUG_SERIAL)

        DEBUG_PRINTLN(
            F("HexTune: S3 WLED released; D1 owns LED DATA."));

#endif
    }


    ///////////////////////////////////////////////////////////////////////////
    // Acquire WLED / S3 Owns Strip
    ///////////////////////////////////////////////////////////////////////////

    void acquireWLED()
    {
        ///////////////////////////////////////////////////////////////////////////
        // Claim GPIO5
        ///////////////////////////////////////////////////////////////////////////

        /*
         * D1 has already released GPIO4 before GPIO8 is asserted HIGH.
         */
        pinMode(
            WLED_DATA_PIN,
            OUTPUT);


        ///////////////////////////////////////////////////////////////////////////
        // Resume WLED Strip Service
        ///////////////////////////////////////////////////////////////////////////

        strip.resume();


        ///////////////////////////////////////////////////////////////////////////
        // Force First WLED Frame
        ///////////////////////////////////////////////////////////////////////////

        strip.trigger();


#if defined(HT_DEBUG_SERIAL)

        DEBUG_PRINTLN(
            F("HexTune: S3 WLED acquired; S3 owns LED DATA."));

#endif
    }


public:

    ///////////////////////////////////////////////////////////////////////////
    // Usermod ID
    ///////////////////////////////////////////////////////////////////////////

    uint16_t getId() override
    {
        return USERMOD_ID_UNSPECIFIED;
    }
};


///////////////////////////////////////////////////////////////////////////////
// Global Usermod Instance
///////////////////////////////////////////////////////////////////////////////

static HexTuneOwnership
    hexTuneOwnership;


REGISTER_USERMOD(
    hexTuneOwnership);
