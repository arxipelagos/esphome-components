#include "esphome.h"
#include "wmbus.h"

namespace esphome {
namespace wmbus {

class DriverEngelmannWater : public WMBusDriver {
 public:
  DriverEngelmannWater() = default;
  ~DriverEngelmannWater() override = default;

  // Controller calls this to see if the driver matches an incoming frame
  bool matches(const WMBusTelegram &t) const override {
    // Manufacturer = Engelmann (EFE = 0xC514)
    if (t.manufacturer != 0xC514) {
      return false;
    }

    // We're only interested in Water device (0x07) with Compact Frame CI 0x73
    if (t.device == 0x07 && t.ci_field == 0x73) {
      return true;
    }

    return false;
  }

  // Decode payload into structured fields
  void decode(WMBusTelegram &t) override {
    if (!t.is_compact_frame) {
      ESP_LOGW(TAG, "EngelmannWater: not a compact frame, skipping");
      return;
    }

    for (auto &record : t.records) {
      uint32_t dif_vif = record.get_dif_vif();

      switch (dif_vif) {
        // 0x046D: Time & Date
        case 0x046D: {
          time_t ts = record.get_datetime_f();
          t.add_uint_field("timestamp", (uint64_t)ts, WMBUS_FIELD_FLAG_TIMESTAMP);
          break;
        }

        // 0x0413: Volume in m^3 (exponent 0.001)
        case 0x0413: {
          double volume_m3 = record.get_int32() * 0.001;
          t.add_double_field("total", volume_m3);
          break;
        }

        // 0x01FD17: Error Flags (raw 8 bits)
        case 0x01FD17: {
          uint8_t err = record.get_uint8();
          t.add_uint_field("error", err);
          break;
        }

        default:
          // unhandled field
          break;
      }
    }
  }

  const char *get_name() const override { return "EngelmannWater"; }
};

}  // namespace wmbus
}  // namespace esphome

// Register the driver
static WMBusDriverRegistration<esphome::wmbus::DriverEngelmannWater> _engelmann_water_driver;
