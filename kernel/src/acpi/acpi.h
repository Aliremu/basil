#pragma once

#include "libk/acpi.h"
#include "mm/address.h"
#include <memory>
#include <string_view>

namespace acpi
{
// Validates the RSDP and the XSDT (or RSDT) it points to, and remembers which
// one to search. Panics if either is broken: without ACPI there is no APIC.
auto init(mm::PhysicalAddress rsdp) -> void;

// The table with this signature ("APIC", "MCFG", ...), mapped and checksummed,
// or nullptr if the firmware has none.
[[nodiscard]] auto find(std::string_view signature) -> const libk::acpi::SdtHeader*;

// The same, viewed as its struct: find<libk::acpi::Madt>(). nullptr if the
// table is missing or shorter than T.
template <typename T>
[[nodiscard]] auto find() -> const T*
{
    const auto* header = find(T::signature);
    if (header == nullptr || header->length < sizeof(T))
        return nullptr;
    return std::start_lifetime_as<T>(header);
}
}
