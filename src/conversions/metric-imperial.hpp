#pragma once

namespace conversions
{
    struct metric_imperial
    {
        static double meters_to_feet(double meters) { return meters * 3.28084; }
        static double feet_to_meters(double feet) { return feet / 3.28084; }
    };
} // namespace conversions
