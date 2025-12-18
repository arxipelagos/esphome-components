#include "driver.h"
#include "wmbus.h"

using namespace std;

class DriverEngelmannFAW : public Driver
{
public:
    DriverEngelmannFAW(MeterInfo &mi)
        : Driver(mi, "engelmann-faw") {}

    // ------------------------------------------------------------
    // Detect supported meters
    // ------------------------------------------------------------
    bool detect(Telegram *t) override
    {
        // Manufacturer: Engelmann (EFE)
        if (t->manufacturer != MANUFACTURER_ENGELMANN)
            return false;

        // Original FAW (Heat)
        if (t->device == 0x04)
            return true;

        // 🔽 Erweiterung: Engelmann Water, Compact Frame
        if (t->device == 0x07 && t->ci_field == 0x73)
            return true;

        return false;
    }

    // ------------------------------------------------------------
    // Decode payload
    // ------------------------------------------------------------
    void processContent(Telegram *t) override
    {
        // ========================================================
        // 046D – Date & Time (F-format)
        // ========================================================
        if (extractDV(t, "046D", &timestamp_))
        {
            addNumericField(
                "timestamp",
                Unit::Second,
                timestamp_,
                Quantity::Time,
                VifScaling::None);
        }

        // ========================================================
        // 0413 – Volume (m³, exponent 0.001)
        // ========================================================
        if (extractDV(t, "0413", &volume_raw_))
        {
            double volume_m3 = volume_raw_ * 0.001;

            addNumericField(
                "total",
                Unit::CubicMeter,
                volume_m3,
                Quantity::Volume,
                VifScaling::None);
        }

        // ========================================================
        // 01FD17 – Error flags
        // ========================================================
        if (extractDV(t, "01FD17", &error_))
        {
            addNumericField(
                "error",
                Unit::None,
                error_,
                Quantity::None,
                VifScaling::None);
        }
    }

private:
    double timestamp_   = 0;
    double volume_raw_  = 0;
    double error_       = 0;
};

// ------------------------------------------------------------
// Driver registration
// ------------------------------------------------------------
static DriverRegister<DriverEngelmannFAW> register_engelmann_faw;
