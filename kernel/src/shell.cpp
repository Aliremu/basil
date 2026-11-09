#include "shell.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pit.h"
#include "core/print.h"
#include "gfx/console.h"
#include "gfx/framebuffer.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>

namespace basil
{
namespace
{
constexpr auto art = std::array<std::string_view, 25>{
    // "⠀⠀⣠⣿⣿⠿⠿⠿⠷⠶⣦⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣤⡶⠶⠿⠿⠿⢿⣿⣧⡀⠀⠀",
    // "⠲⣿⡟⠉⠀⠀⢀⣤⣶⣶⣶⣍⠛⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠟⢋⣴⣶⣶⣶⣄⠀⠀⠈⠙⣿⡷⠂",
    // "⠀⢹⡇⠀⠀⠀⠈⠈⢰⣤⣾⡟⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢽⣦⣴⠞⠙⠀⠀⠀⠀⣿⠃⠀",
    // "⠀⠀⢷⠀⠀⠀⠀⠘⠂⠉⠁⠠⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠣⠂⠉⠁⠀⠈⠀⠀⠀⢰⠃⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⢀⠀⢀⠔⢋⡠⢀⡀⠀⣀⠄⣀⠔⠀⠀⠀⠀⠀⠀⠀⠀⢀⡤⠀⠀⠀⠀⠀",
    // "⠀⢀⠔⢀⡤⠀⡠⠒⣀⠝⡡⠚⢁⠔⢋⠔⠉⡠⠞⡡⠞⠁⡠⠚⠀⣀⠄⠉⢁⠤⠊⠁⡠⠔⠉⠀⠀⠀",
    // "⠐⡡⠚⢁⠔⠉⡠⢊⠵⠊⡠⠖⡡⠚⢁⠤⢊⡠⠊⢀⠔⠊⢀⠴⠊⣠⠔⠊⢁⡤⠒⢉⠤⠂⠀⠀⠀⠀",
    // "⠈⠀⠞⠁⡴⠊⠀⠁⠠⠊⠔⠋⢀⠔⢁⠔⠉⣠⠾⣁⠀⠐⠁⠒⠉⠀⠤⠚⠁⠀⠊⠁⠀⠀⠀⠀⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠁⢀⣌⣀⣀⣀⣡⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡾⠿⢿⣿⡿⠿⢿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠆⠀⠀⠀⠀⣂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⢄⠀⠀⠀⠀⠀⢀⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠑⠀⠀⠀⠐⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
    // "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
    //
    "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣄⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀", "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠙⠳⣄⠀⠀⠀⠀⠀⠀⠀",
    "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀⠀⠀⠀⠀⠀⠀", "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣠⣤⣤⣤⣤⣄⣀⣀⠀⠀⠀⠀⠀⣠⠎⠀⠀⠀⠀⠀⠀⠀",
    "⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣶⣖⡉⠀⠀⠀⠀⠀⠀⠀⠀⠀", "⠀⠀⢠⣄⣀⣠⣤⣤⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣄⠀⠀⠀⠀⠀⠀⠀",
    "⠀⠀⠀⠉⠻⣿⣿⣿⣿⣿⣿⣯⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠀⠀⠀⠀⠀", "⠀⠀⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀⠀",
    "⠀⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀", "⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀",
    "⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠀", "⠠⣾⣿⢿⣿⣿⣿⣿⡿⠁⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠉⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⠉⠀",
    "⠀⠀⠀⢸⣿⣿⣿⡿⠑⠊⣿⣿⡿⠿⠛⠛⠙⠛⣻⣿⣿⣄⡻⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀", "⠀⠀⠀⢸⣿⣿⣿⡗⠾⠛⠉⠉⠀⠀⠀⠀⠀⠀⠈⠉⠉⠙⠛⠛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀",
    "⠀⠀⠀⢸⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⠟⠛⠻⣿⣿⣿⣿⣿⣿⡄", "⠀⠀⠀⠀⣿⣿⣿⣇⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⢶⡋⠳⢸⣿⣿⣿⣿⣿⣇",
    "⠀⠂⠀⠀⠘⣿⣿⣿⡀⠀⠀⠀⠀⠀⠉⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⡗⠚⢁⣠⣾⣿⣿⣿⣿⣿⣿", "⠀⠉⠀⠀⠀⠈⣻⣿⣿⣦⣄⣀⡀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣾⣿⣿⣿⣷⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿",
    "⢺⣿⠤⠿⢿⣿⣿⣿⣿⣿⣿⣷⣶⡄⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",    "⠀⠀⠀⠀⠀⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⢿⣿⣿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
    "⠀⠀⠀⠀⠀⠀⣸⣿⣿⣿⣿⣿⣿⣿⣿⠟⠁⠀⣀⡠⠜⠋⠁⠀⠀⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿", "⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⡿⠛⣠⣟⣁⠤⠖⠋⠁⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⡿",
    "⠀⠀⠀⠀⠀⠀⣿⣿⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣿⣿⡟⢸⠿⠃", "⠀⠀⠀⠀⠀⠀⢸⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢦⠀⠀⠀⠀⠀⠀",
    "⠀⠀⠀⠀⠀⠀⡼⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣆⠀⠀⠀⠀⠀",
};

[[nodiscard]] auto cpu_brand() -> std::array<char, 49>
{
    auto brand = std::array<char, 49>{};

    auto max_leaf = uint32_t{};
    __asm__ volatile("cpuid" : "=a"(max_leaf) : "a"(0x80000000) : "ebx", "ecx", "edx");
    if (max_leaf < 0x80000004)
        return brand;

    for (auto i = 0u; i < 3; ++i)
    {
        auto regs = std::array<uint32_t, 4>{};
        __asm__ volatile("cpuid"
                         : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                         : "a"(0x80000002 + i), "c"(0));
        std::ranges::copy(std::bit_cast<std::array<char, 16>>(regs), brand.begin() + i * 16);
    }

    return brand;
}
}

Shell::Shell(std::size_t cols)
    : line_{}
    , cols_{cols}
{
    refresh();
}

auto Shell::on_key(const keyboard::KeyEvent& key) -> void
{
    if (!key.pressed)
        return;

    auto recall = [&]
    {
        line_.clear();
        if (history_idx_ == 0)
            return;

        const auto it = std::next(history_.rbegin(), history_idx_ - 1);
        for (const auto c : *it)
        {
            if (c != '\0')
                line_.insert(c);
        }
    };

    if (const auto c = keyboard::read_char(key))
    {
        if (*c == '\n')
            return submit();

        if (*c == '\b')
            line_.backspace();
        else
            line_.insert(*c);
    }
    else if (key.key == keyboard::Scancode::LeftArrow)
    {
        line_.left();
    }
    else if (key.key == keyboard::Scancode::RightArrow)
    {
        line_.right();
    }
    else if (key.key == keyboard::Scancode::UpArrow)
    {
        if (history_idx_ == history_.size())
            return;

        ++history_idx_;
        recall();
    }
    else if (key.key == keyboard::Scancode::DownArrow)
    {
        if (history_idx_ == 0)
            return;

        --history_idx_;
        recall();
    }
    else
    {
        return;
    }

    refresh();
}

auto Shell::run(std::string_view line) -> void
{
    const auto it = line.find(' ');
    const auto cmd = line.substr(0, it);
    const auto rest = line.substr(it + 1);

    if (cmd == "help")
    {
        kernel::kprintf(
            "Commands:\n\thelp - returns this screen\n\tuptime - returns the system's current uptime\n\techo - prints "
            "back the input\n\tneofetch - neofetch\n\treboot - reboots the system\n\t");
    }
    else if (cmd == "uptime")
    {
        kernel::kprintf("uptime: {}\n", arch::uptime_ticks());
    }
    else if (cmd == "echo")
    {
        if (it != std::string_view::npos)
            kernel::kprintf("{}\n", rest);
    }
    else if (cmd == "neofetch")
    {
        neofetch();
    }
    else if (cmd == "reboot")
    {
        for (auto i = 0; i < 100000 && (arch::inb(0x64) & 0x02); ++i)
        {
        }
        arch::outb(0x64, 0xFE);
    }
    else if (cmd == "pagefault")
    {
        *reinterpret_cast<volatile uint64_t*>(0xDEAD0000) = 1;
    }
    else if (cmd == "doublefault")
    {
        __asm__ volatile("mov $0xDEAD0000, %rsp\n\tpushq $0");
    }
    else
    {
        kernel::kprintf("\x1b[31munknown command\x1b[37m\n");
    }
}

auto Shell::refresh() -> void
{
    const auto width = cols_ - prompt.size() - 1;
    const auto cursor = line_.cursor();

    offset_ = std::clamp(offset_, cursor > width ? cursor - width : 0, cursor);

    const auto visible = line_.text().substr(offset_, width);
    kernel::kprintf("\r\x1b[K{}{}", prompt, visible);

    if (const auto back = offset_ + visible.size() - cursor; back > 0)
        kernel::kprintf("\x1b[{}D", back);
}

auto Shell::submit() -> void
{
    kernel::kprintf("\n");
    offset_ = 0;
    run(line_.text());
    if (!line_.text().empty())
    {
        if (history_.full())
            history_.pop_front();

        auto arr = std::array<char, 256>{};
        std::copy_n(line_.text().begin(), line_.text().size(), arr.begin());
        history_.push_back(std::move(arr));
    }
    history_idx_ = 0;
    line_.clear();
    refresh();
}

auto Shell::neofetch() -> void
{
    auto n = 0uz;
    auto row = [&n](std::string_view key) { kernel::kprintf("\x1b[32m{}  {}\x1b[0m", art[n++], key); };

    const auto brand = cpu_brand();
    auto cpu = std::string_view{brand.data()};
    cpu.remove_prefix(std::min(cpu.find_first_not_of(' '), cpu.size()));

    const auto seconds = arch::uptime_ticks() / 100;
    const auto& screen = gfx::screen();

    row("root\x1b[0m@\x1b[32mbasil");
    kernel::kprintf("\n");
    row("");
    kernel::kprintf("----------\n");
    row("OS: ");
    kernel::kprintf("BasilOS x86_64\n");
    row("Kernel: ");
    kernel::kprintf("basil 0.1.0 (GCC {})\n", std::string_view{__VERSION__});
    row("Uptime: ");
    kernel::kprintf("{}m {}s\n", seconds / 60, seconds % 60);
    row("Shell: ");
    kernel::kprintf("basil shell\n");
    row("Resolution: ");
    kernel::kprintf("{}x{}\n", screen.width, screen.height);
    row("Terminal: ");
    kernel::kprintf("basil console ({}x{})\n", gfx::Console::cols(), gfx::Console::rows());
    row("CPU: ");
    kernel::kprintf("{}\n", cpu.empty() ? std::string_view{"unknown"} : cpu);
    row("Memory: ");
    kernel::kprintf("??? (no allocator yet)\n");

    row("");
    kernel::kprintf("\n");
    row("");
    for (auto color = 30; color <= 37; ++color)
        kernel::kprintf("\x1b[{}m███", color);
    kernel::kprintf("\x1b[0m\n");

    while (n < art.size())
    {
        row("");
        kernel::kprintf("\n");
    }
}
}
