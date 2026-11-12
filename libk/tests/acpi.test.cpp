#include "libk/acpi.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// These fail until checksum_ok and for_each_madt_entry are filled in.

namespace
{
using namespace libk::acpi;

template <typename T>
auto append(std::vector<std::byte>& bytes, const T& value) -> void
{
    const auto* p = reinterpret_cast<const std::byte*>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(T));
}

// A MADT with the given entries after it, header length set and checksum fixed.
auto make_madt(const std::vector<std::byte>& entries) -> std::vector<std::byte>
{
    auto madt = Madt{};
    std::memcpy(madt.header.signature.data(), "APIC", 4);
    madt.header.length = static_cast<uint32_t>(sizeof(Madt) + entries.size());
    madt.local_apic_address = 0xFEE0'0000;

    auto bytes = std::vector<std::byte>{};
    append(bytes, madt);
    bytes.insert(bytes.end(), entries.begin(), entries.end());

    auto sum = uint8_t{};
    for (auto b : bytes)
        sum += static_cast<uint8_t>(b);
    bytes[offsetof(SdtHeader, checksum)] = std::byte(-sum);
    return bytes;
}

auto io_apic(uint32_t address, uint32_t gsi_base) -> MadtIoApic
{
    return {{MadtType::IoApic, sizeof(MadtIoApic)}, 0, 0, address, gsi_base};
}

auto override_(uint8_t source, uint32_t gsi, uint16_t flags) -> MadtInterruptSourceOverride
{
    return {{MadtType::InterruptSourceOverride, sizeof(MadtInterruptSourceOverride)}, 0, source, gsi, flags};
}
}

TEST(AcpiChecksum, AcceptsBytesThatSumToZero)
{
    const auto bytes = std::array{std::byte{0x10}, std::byte{0x20}, std::byte{0xD0}};
    EXPECT_TRUE(checksum_ok(bytes));
}

TEST(AcpiChecksum, RejectsBytesThatDoNot)
{
    const auto bytes = std::array{std::byte{0x10}, std::byte{0x20}, std::byte{0xD1}};
    EXPECT_FALSE(checksum_ok(bytes));
}

TEST(AcpiChecksum, WrapsPastTwoFiftyFive)
{
    // 0xFF * 3 = 0x2FD; 0x2FD + 0x03 = 0x300, which is 0 mod 256.
    const auto bytes = std::array{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x03}};
    EXPECT_TRUE(checksum_ok(bytes));
}

TEST(AcpiChecksum, MadtBuiltHereIsValid)
{
    EXPECT_TRUE(checksum_ok(make_madt({})));
}

TEST(AcpiMadt, VisitsEntriesInOrder)
{
    auto entries = std::vector<std::byte>{};
    append(entries, io_apic(0xFEC0'0000, 0));
    append(entries, override_(0, 2, 0));
    append(entries, override_(9, 9, inti_active_low | inti_level_triggered));
    const auto madt = make_madt(entries);

    auto seen = std::vector<MadtType>{};
    auto gsis = std::vector<uint32_t>{};
    for_each_madt_entry(
        madt,
        [&](const MadtEntryHeader& entry)
        {
            seen.push_back(entry.type);
            if (entry.type == MadtType::InterruptSourceOverride)
                gsis.push_back(reinterpret_cast<const MadtInterruptSourceOverride&>(entry).gsi);
        });

    EXPECT_EQ(
        seen, (std::vector{MadtType::IoApic, MadtType::InterruptSourceOverride, MadtType::InterruptSourceOverride}));
    EXPECT_EQ(gsis, (std::vector<uint32_t>{2, 9}));
}

TEST(AcpiMadt, SkipsUnknownTypesByLength)
{
    auto entries = std::vector<std::byte>{};
    // An unknown type 0x7F, 7 bytes long, then an I/O APIC.
    entries.insert(entries.end(), {std::byte{0x7F}, std::byte{7}, {}, {}, {}, {}, {}});
    append(entries, io_apic(0xFEC0'0000, 0));
    const auto madt = make_madt(entries);

    auto seen = std::vector<uint8_t>{};
    for_each_madt_entry(madt, [&](const MadtEntryHeader& entry) { seen.push_back(std::to_underlying(entry.type)); });
    EXPECT_EQ(seen, (std::vector<uint8_t>{0x7F, 1}));
}

TEST(AcpiMadt, StopsAtZeroLengthEntry)
{
    auto entries = std::vector<std::byte>{};
    append(entries, io_apic(0xFEC0'0000, 0));
    entries.insert(entries.end(), {std::byte{1}, std::byte{0}}); // length 0: would loop forever
    const auto madt = make_madt(entries);

    auto count = 0;
    for_each_madt_entry(madt, [&](const MadtEntryHeader&) { ++count; });
    EXPECT_EQ(count, 1);
}

TEST(AcpiMadt, StopsAtEntryRunningPastTheEnd)
{
    auto entries = std::vector<std::byte>{};
    append(entries, io_apic(0xFEC0'0000, 0));
    entries.insert(entries.end(), {std::byte{1}, std::byte{12}, {}, {}}); // claims 12, has 4
    const auto madt = make_madt(entries);

    auto count = 0;
    for_each_madt_entry(madt, [&](const MadtEntryHeader&) { ++count; });
    EXPECT_EQ(count, 1);
}
