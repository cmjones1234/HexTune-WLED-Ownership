```cpp
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


class HexTuneOwnership : public Usermod
{
public:

    ///////////////////////////////////////////////////////////////////////////
    // Configuration
    ///////////////////////////////////////////////////////////////////////////

    static constexpr uint8_t OWNER_INPUT_PIN = 8U;


    ///////////////////////////////////////////////////////////////////////////
    // State
    ///////////////////////////////////////////////////////////////////////////

    bool m_initialized = false;
    bool m_lastRemoteOwnership = false;


    ///////////////////////////////////////////////////////////////////////////
    // Setup
    ///////////////////////////////////////////////////////////////////////////

    void setup() override
    {
        /*
         * GPIO8 is driven by D1 GPIO23.
         *
         * The pulldown establishes the safe default:
         *
         *     LOW = D1 owns LEDs
         *
         * This also prevents the S3 from briefly assuming ownership while
         * the D1 is still booting.
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


        /*
         * WLED has already created its normal LED buses by the time Usermod
         * setup() executes.
         *
         * If D1 owns the strip at startup, release WLED's buses now.
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

    ///////////////////////////////////////////////////////////////////////////
    // Ownership State Machine
    ///////////////////////////////////////////////////////////////////////////

    void applyOwnership(
        bool remoteOwnership)
    {
        if (remoteOwnership)
        {
            /*
             * GPIO8 HIGH:
             *
             * S3/WLED owns the physical LED strip.
             */
            acquireWLED();

            return;
        }


        /*
         * GPIO8 LOW:
         *
         * D1 owns the physical LED strip.
         */
        releaseWLED();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Release WLED Bus
    ///////////////////////////////////////////////////////////////////////////

    void releaseWLED()
    {
        /*
         * Stop WLED from producing LED data before releasing the buses.
         *
         * BusManager::off() changes the WLED output state without destroying
         * the WS2812FX object or its segment/preset state.
         */
        BusManager::off();


        /*
         * Release the physical LED buses.
         *
         * This is the important operation. BusManager owns the underlying
         * digital LED driver resources and GPIO allocation.
         *
         * DO NOT destroy/reconstruct 'strip'.
         */
        BusManager::removeAll();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Acquire WLED Bus
    ///////////////////////////////////////////////////////////////////////////

    void acquireWLED()
    {
        /*
         * Preserve WLED's existing segment configuration before rebuilding
         * the physical bus.
         */
        const bool aligned =
            strip.checkSegmentAlignment();


        /*
         * Recreate the LED buses from WLED's existing bus configuration.
         *
         * This is the same fundamental operation WLED performs when its
         * normal bus reinitialization path executes.
         */
        strip.finalizeInit();


        /*
         * Restore segment mapping exactly as WLED does after bus
         * reinitialization.
         */
        if (aligned)
        {
            strip.makeAutoSegments();
        }
        else
        {
            strip.fixInvalidSegments();
        }


        /*
         * Restore WLED's configured brightness to the newly created buses.
         */
        BusManager::setBrightness(
            scaledBri(bri));


        /*
         * Request a fresh frame.
         *
         * Do not manually configure the LED GPIO here. The WLED bus driver
         * owns and initializes the GPIO as part of finalizeInit().
         */
        strip.trigger();


        /*
         * Send the current frame immediately.
         */
        strip.show();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Identification
    ///////////////////////////////////////////////////////////////////////////

public:

    uint16_t getId() override
    {
        return USERMOD_ID_UNSPECIFIED;
    }
};


static HexTuneOwnership hexTuneOwnership;


REGISTER_USERMOD(
    hexTuneOwnership);
```
