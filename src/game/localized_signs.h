#pragma once
#include "engine/texture.h"

namespace OpenBlood2
{
    class Localization;
    class LocalizedFont;

    void localize_sign_texture(const Resources&, std::string_view path, Texture&, const Localization&,
                               const LocalizedFont&);
    // The catalog and font must outlive the presentation that uses the callback.
    TextureDecoration sign_texture_decoration(const Localization&, const LocalizedFont&);
} // namespace OpenBlood2
