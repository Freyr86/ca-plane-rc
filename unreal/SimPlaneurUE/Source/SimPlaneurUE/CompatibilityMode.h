#pragma once

namespace CompatibilityMode
{
    bool IsEnabled();
    // Session-only render settings. Never change or save UGameUserSettings.
    void Apply();
}
