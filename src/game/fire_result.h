#pragma once
// Common result vocabulary for local fire delivery and client prediction.
// Network acknowledgements encode their own bounded, versioned values.
enum class FireResult
{
    Fired,
    Dry,
    Invalid,
    Unavailable,
    DeliveryFailed,
    Blocked
};
