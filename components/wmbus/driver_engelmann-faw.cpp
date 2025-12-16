/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)
*/

#include <optional>
#include "meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo &di)
    {
        di.setName("engelmann-faw");

        // C1 liefert nur aktuellen Wert
        di.setDefaultFields("name,id,status,consumption,timestamp");

        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);

        di.addDetection(MANUFACTURER_EFE, 0x07, 0x00);

        di.setConstructor([](MeterInfo &mi, DriverInfo &di)
        {
            return std::shared_ptr<Meter>(new Driver(mi, di));
        });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di)
        : MeterCommonImplementation(mi, di)
    {
        /* =========================
         * STATUS
         * ========================= */
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build().set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger,
                        MaskBits(0xff),
                        "OK",
                        {
                            {0x01, "VOLUME_DETECTION_COILS_DEFECT"},
                            {0x02, "RESET"},
                            {0x04, "CRC_ERROR"},
                            {0x08, "REMOVAL_DETECTED"},
                            {0x10, "MAGNETIC_MANIPULATION"},
                            {0x20, "LEAKAGE"},
                            {0x40, "BLOCKED"},
                            {0x80, "REVERSE_FLOW"},
                        }
                    },
                },
            });

        /* =========================
         * AKTUELLER VERBRAUCH
         * C1 KOMPAKT – AES MODE 5
         * ========================= */
        addNumericField(
            "consumption",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "Current water consumption (C1 compact telegram, AES mode 5).",
            Unit::CubicMeter,
            [](Telegram &t) -> std::optional<double>
            {
                // In dieser Codebasis ist payload() bereits entschlüsselt
                const auto &p = t.payload();

                // Python: data[33:37]
                if (p.size() < 37)
                    return std::nullopt;

                uint32_t raw =
                    static_cast<uint32_t>(p[33]) |
                    (static_cast<uint32_t>(p[34]) << 8) |
                    (static_cast<uint32_t>(p[35]) << 16) |
                    (static_cast<uint32_t>(p[36]) << 24);

                // Liter → m³
                return static_cast<double>(raw) / 1000.0;
            });

        /* =========================
         * ARCHIVWERTE (T1)
         * ========================= */
        addStringFieldWithExtractor(
            "reporting_date",
            "The reporting date of the last billing period.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Date)
                .set(StorageNr(1)));

        addNumericFieldWithExtractor(
            "consumption_at_reporting_date",
            "The water consumption at the last billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Volume)
                .set(StorageNr(1)));

        for (int i = 2; i <= 16; ++i)
        {
            std::string name, info;
            strprintf(&name, "consumption_%d_months_ago", i - 1);
            strprintf(&info, "Water consumption %d month(s) ago.", i - 1);

            addNumericFieldWithExtractor(
                name,
                info,
                DEFAULT_PRINT_PROPERTIES,
                Quantity::Volume,
                VifScaling::Auto,
                DifSignedness::Signed,
                FieldMatcher::build()
                    .set(MeasurementType::Instantaneous)
                    .set(VIFRange::Volume)
                    .set(StorageNr(i)));
        }
    }
}
