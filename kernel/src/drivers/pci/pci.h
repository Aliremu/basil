#pragma once

#include <cstdint>
#include <inplace_vector>

namespace pci
{
// One PCI function: the unit that has its own 4 KiB configuration space. A
// physical card ("device") can expose up to 8 functions.
struct Function
{
    uint8_t bus, device, function;
    uint16_t vendor_id, device_id;
    uint8_t class_code, subclass, prog_if;
};

// Finds the ECAM windows in the ACPI MCFG table and maps them. Call after
// acpi::init.
auto init() -> void;

// Every function on every bus, in bus/device/function order.
[[nodiscard]] auto enumerate() -> std::inplace_vector<Function, 64>;

// Configuration space access. `offset` must be 4-byte aligned.
[[nodiscard]] auto read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) -> uint32_t;
auto write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) -> void;
}
