#pragma once

#include "gfx/framebuffer.h"
#include <limine.h>
#include <optional>

// What the kernel learns from the Limine boot protocol. The request objects
// themselves live in limine.cpp; only the answers are exposed here.
namespace boot
{
auto base_revision_supported() -> bool;
auto framebuffer() -> std::optional<gfx::Framebuffer>;
auto rsdp() -> void;
}
