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

    // ------------------------------------------------------------------
    // Driver registration
    // ------------------------------------------------------------------
    static bool ok = registerDriver([](DriverInfo& di)
    {
        di.setName("engelmann-faw");
        di.setDefaultFields(
            "name,id,status,error_flags,reporting_date,"
            "consumption_at_reporting_date_m3,total_m3,timestamp");

        di.addLinkMode(LinkMode::T1);

        // Engelmann Water (0x07) – CI is intentionally NOT filtered
        di.addDetection(MANUFACTURER_EFE, 0x07, 0x00);

        di.setConstructor(
            [](MeterInfo& mi, DriverInfo& di)
            {
                return shared_ptr<Meter>(new Driver(mi, di));
            });
    });

    // ------------------------------------------------------------------
    // Constructor
    // ------------------------------------------------------------------
    Driver::Driver(MeterInfo &mi, DriverInfo &di) :
        MeterCommonImplementation(mi, di)
    {
        // ==============================================================
        // STATUS / ERROR FLAGS  (01FD17)
        // ==============================================================
        addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES |
                PrintProperty::STATUS |
                PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger,
                        MaskBits(0xff),
                        "OK",
                        {
                            { 0x01, "VOLUME_DETECTION_COILS_DEFECT" },
                            { 0x02, "RESET" },
                            { 0x04, "CRC_ERROR" },
                            { 0x08, "REMOVAL_DETECTED" },
                            { 0x10, "MAGNETIC_MANIPULATION" },
                            { 0x20, "LEAKAGE" },
                            { 0x40, "BLOCKED" },
                            { 0x80, "REVERSE_FLOW" },
                        }
                    },
                },
            });

        // ==============================================================
        // REPORTING DATE (billing, StorageNr = 1)
        // ==============================================================
        addStringFieldWithExtractor(
            "reporting_date",
            "The reporting date of the last billing period.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Date)
                .set(StorageNr(1)));

        // ==============================================================
        // CONSUMPTION AT REPORTING DATE
        // ==============================================================
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

        // ==============================================================
        // HISTORIC MONTHLY VALUES (FAW classic)
        // ==============================================================
        for (int i = 2; i <= 16; ++i)
        {
            string name, info;
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

        // ==============================================================
        // 🔽 ERWEITERUNG: COMPACT FRAME SUPPORT
        // ==============================================================

        // --------------------------------------------------------------
        // TOTAL VOLUME (0413, StorageNr = 0)
        // --------------------------------------------------------------
        addNumericFieldWithExtractor(
            "total",
            "Total water volume (compact frame).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Volume)
                .set(StorageNr(0)));

        // --------------------------------------------------------------
        // TIMESTAMP (046D, DateTime, Instantaneous)
        // --------------------------------------------------------------
        addNumericFieldWithExtractor(
             "timestamp",
             "Meter timestamp (compact frame).",
             DEFAULT_PRINT_PROPERTIES,
             Quantity::Time,
             VifScaling::None,
             DifSignedness::Signed,
             FieldMatcher::build()
                 .set(MeasurementType::Instantaneous)
                 .set(VIFRange::DateTime));

    }
}
