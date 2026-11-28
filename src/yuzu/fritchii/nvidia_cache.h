// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QWidget;

namespace FritchiiUi {

/// Cervin (owner 2026-11-28): NVIDIA's driver keeps compiled shaders in a disk cache whose
/// Vulkan part stayed at ~1 GB here, far below Ocarina of Time's ~120,000 prepared pipelines, so
/// the driver dropped prepared ones and OoT compiled them again in play (up to 1.7 s each).
/// __GL_SHADER_DISK_CACHE_SIZE (main.cpp) is not read by the Windows driver. This sets the
/// driver's own "Shader disk cache maximum size" (NVAPI DRS setting 0x00AC8497, in MB) to 8 GB in
/// an application profile for Cervin.exe only, after asking once. Nothing changes for other
/// programs or the global profile. Windows with an NVIDIA driver only; CERVIN_NVIDIA_CACHE=0 never
/// asks or changes anything.
void OfferNvidiaShaderCache(QWidget* parent);

} // namespace FritchiiUi
