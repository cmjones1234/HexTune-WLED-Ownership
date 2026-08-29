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
 *     WLED releases its physical LED bus.
 *
 * GPIO8 HIGH:
 *     S3/WLED owns the physical WS2812B strip.
 *     WLED restores its configured physical LED bus.
 *
 * WLED hardware defaults are supplied by HexTune-WLED-Build:
 *
 *     DATA_PINS    = 5
 *     PIXEL_COUNTS = 7
 *     LED_TYPES    = TYPE_WS2812_RGB
 *
 * WLED remains responsible for persistent LED configuration.
 * This UserMod is responsible only for physical ownership handoff.
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
    // Saved Bus Record
    //
    // We intentionally do NOT store BusConfig objects directly.
    // BusConfig has no default constructor in this WLED build.
    ///////////////////////////////////////////////////////////////////////////

    struct SavedBus
    {
        uint8_t type = TYPE_WS2812_RGB;

        uint8_t pins[OUTPUT_MAX_PINS] =
        {
            255,
            255,
            255,
            255,
            255
        };

        uint16_t start = 0U;

        uint16_t length = 0U;

        uint8_t colorOrder = COL_ORDER_GRB;

        bool reversed = false;

        uint8_t skip = 0U;

        uint8_t autoWhiteMode =
            RGBW_MODE_MANUAL_ONLY;

        uint16_t frequency = 0U;

        uint8_t ledCurrent =
            LED_MILLIAMPS_DEFAULT;

        uint16_t maxCurrent =
            ABL_MILLIAMPS_DEFAULT;

        uint8_t driver = 0U;

        String customText;
    };


    ///////////////////////////////////////////////////////////////////////////
    // State
    ///////////////////////////////////////////////////////////////////////////

    bool m_initialized = false;

    bool m_lastRemoteOwnership = false;

    std::vector<SavedBus> m_savedBuses;


    ///////////////////////////////////////////////////////////////////////////
    // Setup
    ///////////////////////////////////////////////////////////////////////////

    void setup() override
    {
        /*
         * D1 GPIO23 drives S3 GPIO8.
         *
         * LOW  = D1 owns the strip.
         * HIGH = S3/WLED owns the strip.
         *
         * INPUT_PULLDOWN provides the safe D1 ownership state during boot.
         */
        pinMode(
            OWNER_INPUT_PIN,
            INPUT_PULLDOWN);


        const bool remoteOwnership =
            digitalRead(
                OWNER_INPUT_PIN) == HIGH;


        /*
         * WLED has already loaded its persistent configuration and created
         * its normal LED buses before Usermod setup() executes.
         *
         * Capture that configuration before changing physical ownership.
         */
        saveBusConfiguration();


        m_lastRemoteOwnership =
            remoteOwnership;


        m_initialized =
            true;


        /*
         * Apply the ownership state established by D1.
         *
         * If GPIO8 is LOW, WLED releases its physical bus immediately.
         */
        if (!remoteOwnership)
        {
            releaseWLED();
        }
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
                OWNER_INPUT_PIN) == HIGH;


        /*
         * Nothing changed.
         */
        if (remoteOwnership ==
            m_lastRemoteOwnership)
        {
            return;
        }


        /*
         * Record the new ownership state before performing the transition.
         */
        m_lastRemoteOwnership =
            remoteOwnership;


        if (remoteOwnership)
        {
            acquireWLED();
        }
        else
        {
            releaseWLED();
        }
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // Save Current WLED Bus Configuration
    ///////////////////////////////////////////////////////////////////////////

    void saveBusConfiguration()
    {
        m_savedBuses.clear();


        const size_t busCount =
            BusManager::getNumBusses();


        if (busCount == 0U)
        {
            return;
        }


        m_savedBuses.reserve(
            busCount);


        for (size_t index = 0U;
             index < busCount;
             ++index)
        {
            Bus* bus =
                BusManager::getBus(
                    index);


            if (bus == nullptr)
            {
                continue;
            }


            SavedBus saved;


            saved.type =
                bus->getType();


            bus->getPins(
                saved.pins);


            saved.start =
                bus->getStart();


            saved.length =
                bus->getLength();


            saved.colorOrder =
                bus->getColorOrder();


            saved.reversed =
                bus->isReversed();


            saved.skip =
                static_cast<uint8_t>(
                    bus->skippedLeds());


            saved.autoWhiteMode =
                bus->getAutoWhiteMode();


            saved.frequency =
                bus->getFrequency();


            saved.ledCurrent =
                static_cast<uint8_t>(
                    bus->getLEDCurrent());


            saved.maxCurrent =
                bus->getMaxCurrent();


            saved.driver =
                bus->getDriverType();


            saved.customText =
                bus->getCustomText();


            /*
             * BusConfig uses bit 7 to represent the off-refresh flag.
             *
             * Preserve it when reconstructing the configuration.
             */
            if (bus->isOffRefreshRequired())
            {
                saved.type |= 0x80U;
            }


            m_savedBuses.push_back(
                saved);
        }
    }


    ///////////////////////////////////////////////////////////////////////////
    // Release WLED Physical Bus
    ///////////////////////////////////////////////////////////////////////////

    void releaseWLED()
    {
        /*
         * Always capture the current WLED configuration immediately before
         * releasing the bus.
         *
         * This means changes made through the WLED LED Preferences page are
         * retained for the next ownership transition.
         */
        saveBusConfiguration();


        if (m_savedBuses.empty())
        {
            return;
        }


        /*
         * Stop WLED output before releasing the physical bus.
         */
        BusManager::off();


        /*
         * WLED's BusDigital cleanup releases its GPIO ownership and driver
         * resources.
         */
        BusManager::removeAll();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Restore WLED Physical Bus
    ///////////////////////////////////////////////////////////////////////////

    void acquireWLED()
    {
        if (m_savedBuses.empty())
        {
            return;
        }


        /*
         * Reconstruct WLED's temporary BusConfig list.
         *
         * WLED's main loop will process doInitBusses and call:
         *
         *     strip.finalizeInit()
         *
         * which recreates the physical LED buses.
         */
        busConfigs.clear();


        for (const SavedBus& saved :
             m_savedBuses)
        {
            BusConfig config(
                saved.type,
                const_cast<uint8_t*>(
                    saved.pins),
                saved.start,
                saved.length,
                saved.colorOrder,
                saved.reversed,
                saved.skip,
                saved.autoWhiteMode,
                saved.frequency,
                saved.ledCurrent,
                saved.maxCurrent,
                saved.driver,
                saved.customText);


            busConfigs.push_back(
                config);
        }


        /*
         * Ask WLED to perform its native bus reinitialization.
         */
        doInitBusses =
            true;


        /*
         * Request a fresh frame once the bus is restored.
         */
        strip.trigger();
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
