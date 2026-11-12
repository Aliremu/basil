#pragma once

#include "libk/acpi.h"
#include "mm/address.h"
#include <string_view>

namespace acpi
{
// Validates the RSDP and the XSDT (or RSDT) it points to, and remembers which
// one to search. Panics if either is broken: without ACPI there is no APIC.
auto init(mm::PhysicalAddress rsdp) -> void;

// The table with this signature ("APIC", "MCFG", ...), mapped and checksummed,
// or nullptr if the firmware has none. Cast to the matching libk::acpi struct.
[[nodiscard]] auto find(std::string_view signature) -> const libk::acpi::SdtHeader*;
}
