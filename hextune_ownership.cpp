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
 *     WLED releases its physical LED buses.
 *
 * GPIO8 HIGH:
 *     S3/WLED owns the physical WS2812B strip.
 *     WLED restores the previously active LED bus configuration.
 *
 * WLED LED DATA:
 *     Determined by WLED LED Preferences.
 *
 * Design
 * ---------------------------------------------------------------------------
 * The active WLED bus configuration is captured immediately before WLED
 * releases its physical buses.
 *
 * The complete configuration of every active WLED bus is retained by this
 * Usermod while D1 owns the physical strip.
 *
 * When S3/WLED ownership returns, the saved BusConfig objects are restored
 * to WLED's temporary bus configuration list and WLED's native deferred bus
 * initialization mechanism is requested.
 *
 * No LED count or data GPIO is hard-coded here.
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

    std::vector<BusConfig> m_savedBusConfigs;


    ///////////////////////////////////////////////////////////////////////////
    // Setup
    ///////////////////////////////////////////////////////////////////////////

    void setup() override
    {
        /*
         * GPIO8 is driven by D1 GPIO23.
         *
         * LOW  = D1 owns the physical LED strip.
         * HIGH = S3/WLED owns the physical LED strip.
         *
         * Pulldown provides the safe D1 ownership state while the D1 is
         * booting and before GPIO23 has been configured.
         */
        pinMode(
            OWNER_INPUT_PIN,
            INPUT_PULLDOWN);


        const bool remoteOwnership =
            digitalRead(OWNER_INPUT_PIN) == HIGH;


        /*
         * Capture the currently active WLED bus configuration before any
         * ownership transition can release the physical buses.
         */
        saveBusConfiguration();


        m_lastRemoteOwnership =
            remoteOwnership;


        m_initialized =
            true;


        /*
         * WLED has already initialized its normal LED buses by the time
         * Usermod setup() executes.
         *
         * If D1 owns the strip at boot, release WLED's physical buses now.
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
            acquireWLED();

            return;
        }


        releaseWLED();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Save WLED Bus Configuration
    ///////////////////////////////////////////////////////////////////////////

    void saveBusConfiguration()
    {
        /*
         * Rebuild the saved configuration from the currently active WLED
         * buses. This allows future changes to LED count, GPIO, driver,
         * color order, and other bus settings to be retained.
         */
        m_savedBusConfigs.clear();


        const size_t busCount =
            BusManager::getNumBusses();


        for (size_t i = 0;
             i < busCount;
             ++i)
        {
            Bus* bus =
                BusManager::getBus(i);


            if (bus == nullptr)
            {
                continue;
            }


            uint8_t pins[OUTPUT_MAX_PINS] =
            {
                255,
                255,
                255,
                255,
                255
            };


            bus->getPins(
                pins);


            uint8_t busType =
                bus->getType();


            if (bus->isOffRefreshRequired())
            {
                busType |= 0x80U;
            }


            m_savedBusConfigs.emplace_back(
                busType,
                pins,
                bus->getStart(),
                bus->getLength(),
                bus->getColorOrder(),
                bus->isReversed(),
                static_cast<uint8_t>(
                    bus->skippedLeds()),
                bus->getAutoWhiteMode(),
                bus->getFrequency(),
                static_cast<uint8_t>(
                    bus->getLEDCurrent()),
                bus->getMaxCurrent(),
                bus->getDriverType(),
                bus->getCustomText());
        }
    }


    ///////////////////////////////////////////////////////////////////////////
    // Release WLED Physical Buses
    ///////////////////////////////////////////////////////////////////////////

    void releaseWLED()
    {
        /*
         * Capture the current runtime configuration immediately before
         * releasing the buses.
         */
        saveBusConfiguration();


        /*
         * Make certain WLED is not actively driving the strip while D1 owns
         * the physical data line.
         */
        BusManager::off();


        /*
         * Release WLED's physical bus resources.
         *
         * The saved BusConfig objects remain in this Usermod.
         */
        BusManager::removeAll();
    }


    ///////////////////////////////////////////////////////////////////////////
    // Acquire WLED Physical Buses
    ///////////////////////////////////////////////////////////////////////////

    void acquireWLED()
    {
        /*
         * Do not invent an LED count, GPIO, or other configuration.
         *
         * If no valid WLED bus was captured, leave WLED unchanged.
         */
        if (m_savedBusConfigs.empty())
        {
            return;
        }


        /*
         * WLED uses busConfigs as a temporary configuration list consumed
         * by its normal deferred bus initialization path.
         */
        busConfigs.clear();


        for (const BusConfig& savedConfig :
             m_savedBusConfigs)
        {
            busConfigs.push_back(
                savedConfig);
        }


        /*
         * Request WLED's native deferred bus initialization.
         *
         * The WLED main loop will recreate the physical buses from the
         * restored BusConfig objects.
         */
        doInitBusses = true;


        /*
         * Request a fresh frame after the bus has been recreated.
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
