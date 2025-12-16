/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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

        // ✅ Default-Felder: C1 liefert aktuelle Werte, kein Archiv erzwingen
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
         * STATUS / ERROR FLAGS
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
         * C1 – Kompakt-Telegramm
         * AES Mode 5
         * ========================= */
        addNumericField(
            "consumption",
            "Current water consumption (C1 compact telegram, AES mode 5).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            [&](Telegram &t) -> std::optional<double>
            {
                // Entschlüsselter Payload (ohne Link-/TPL-Header)
                const auto &p = t.decryptedPayload();

                // Python-Code nutzt data[33:37] → hier gleicher Offset
                if (p.size() < 37)
                    return std::nullopt;

                uint32_t raw =
                    static_cast<uint32_t>(p[33]) |
                    (static_cast<uint32_t>(p[34]) << 8) |
                    (static_cast<uint32_t>(p[35]) << 16) |
                    (static_cast<uint32_t>(p[36]) << 24);

                // Skalierung: Liter → m³
                return static_cast<double>(raw) / 1000.0;
            });

        /* =========================
         * ARCHIVWERTE (T1 / LANG)
         * bleiben erhalten
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
