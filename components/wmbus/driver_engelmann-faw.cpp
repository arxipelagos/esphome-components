#include "meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo& di)
    {
        di.setName("engelmann-faw");

        // ✅ Default-Felder für C1 (KEIN Erzwingen von Archivwerten)
        di.setDefaultFields("name,id,status,consumption,timestamp");

        // Unterstützte Link-Modes
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);

        // Erkennung
        di.addDetection(MANUFACTURER_EFE, 0x07, 0x00);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di)
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
            FieldMatcher::build()
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

        /* =========================
         * AKTUELLER VERBRAUCH (C1)
         * ========================= */
        addNumericFieldWithExtractor(
            "consumption",
            "Current water consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Volume)
                .set(StorageNr(0))
        );

        /* =========================
         * ARCHIV: LETZTE ABRECHNUNG
         * (optional, nur wenn vorhanden)
         * ========================= */
        addStringFieldWithExtractor(
            "reporting_date",
            "The reporting date of the last billing period.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Date)
                .set(StorageNr(1))
        );

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
                .set(StorageNr(1))
        );

        /* =========================
         * MONATSARCHIV (optional)
         * ========================= */
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
                    .set(StorageNr(i))
            );
        }
    }
}
