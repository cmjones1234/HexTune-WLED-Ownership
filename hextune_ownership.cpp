/******************************************************************************
*

* Project      : HexTune
*
* Module       : hextune_ownership.cpp
*
* Description
* ---
* HexTune physical WS2812B ownership Usermod for WLED.
*
* Hardware Contract
* ---
* D1 GPIO23 -> S3 GPIO8
*
* GPIO8 LOW:
* ```
  D1 owns the physical WS2812B strip.
  ```
* ```
  WLED releases its physical LED buses.
  ```
*
* GPIO8 HIGH:
* ```
  S3/WLED owns the physical WS2812B strip.
  ```
* ```
  WLED restores its previously active LED bus configuration.
  ```
*
* WLED LED DATA:
* ```
  Determined by WLED LED Preferences.
  ```
*
* Design
* ---
* The active WLED BusConfig is captured before the physical bus is released.
*
* The BusConfig is retained by this Usermod while D1 owns the physical strip.
*
* When S3/WLED ownership returns, the saved BusConfig is restored to WLED's
* global bus configuration list and WLED's native deferred bus initialization
* mechanism is requested.
*
* This avoids destroying the global WS2812FX object and avoids hard-coding
* LED count or GPIO configuration into the ownership mechanism.
*

******************************************************************************/

#include "wled.h"
#include "bus_manager.h"

class HexTuneOwnership : public Usermod
{
public:

```
///////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////

static constexpr uint8_t OWNER_INPUT_PIN = 8U;


///////////////////////////////////////////////////////////////////////////
// State
///////////////////////////////////////////////////////////////////////////

bool m_initialized = false;

bool m_lastRemoteOwnership = false;

bool m_busConfigurationSaved = false;

BusConfig m_savedBusConfig;


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
     * Capture the currently configured WLED bus before any ownership
     * transition can remove it.
     */
    saveBusConfiguration();


    m_lastRemoteOwnership =
        remoteOwnership;


    m_initialized =
        true;


    /*
     * WLED has already initialized its normal LED bus by the time the
     * Usermod setup() function is called.
     *
     * If D1 owns the strip at boot, release WLED's physical bus now.
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
```

private:

```
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
     * Only one physical LED bus is currently used by HexTune.
     *
     * The ownership mechanism deliberately does not assume a fixed
     * number of LEDs. The complete WLED BusConfig is retained so that
     * future changes to LED count or other bus parameters remain under
     * WLED's control.
     */
    if (busConfigs.size() == 0)
    {
        m_busConfigurationSaved = false;

        return;
    }


    m_savedBusConfig =
        busConfigs[0];


    m_busConfigurationSaved =
        true;
}


///////////////////////////////////////////////////////////////////////////
// Release WLED Physical Bus
///////////////////////////////////////////////////////////////////////////

void releaseWLED()
{
    /*
     * Make certain WLED is not actively driving the strip while D1 owns
     * the physical data line.
     */
    BusManager::off();


    /*
     * removeAll() releases WLED's physical bus resources, including the
     * GPIO allocation and the underlying digital LED driver.
     *
     * The global WS2812FX object is intentionally preserved.
     */
    BusManager::removeAll();
}


///////////////////////////////////////////////////////////////////////////
// Acquire WLED Physical Bus
///////////////////////////////////////////////////////////////////////////

void acquireWLED()
{
    if (!m_busConfigurationSaved)
    {
        /*
         * No valid configuration was captured.
         *
         * Do not invent a GPIO or LED count. WLED remains in its current
         * state rather than creating an arbitrary bus.
         */
        return;
    }


    /*
     * Restore the saved BusConfig.
     *
     * WLED's normal initialization mechanism consumes this configuration
     * when doInitBusses is processed by the main WLED loop.
     */
    busConfigs.clear();


    busConfigs.push_back(
        m_savedBusConfig);


    /*
     * Request WLED's native deferred bus initialization path.
     *
     * The WLED main loop will subsequently call finalizeInit(), which
     * creates the physical bus using the restored BusConfig.
     */
    doInitBusses = true;


    /*
     * Ask WLED to refresh the current output once the bus has been
     * recreated.
     */
    strip.trigger();
}


///////////////////////////////////////////////////////////////////////////
// Identification
///////////////////////////////////////////////////////////////////////////
```

public:

```
uint16_t getId() override
{
    return USERMOD_ID_UNSPECIFIED;
}
```

};

static HexTuneOwnership hexTuneOwnership;

REGISTER_USERMOD(
hexTuneOwnership);
