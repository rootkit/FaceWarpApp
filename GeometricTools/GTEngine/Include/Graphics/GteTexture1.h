// Geometric Tools LLC, Redmond WA 98052
// Copyright (c) 1998-2015
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 2.0.0 (2015/09/23)

#pragma once

#include <Graphics/GteTextureSingle.h>

namespace gte
{

class GTE_IMPEXP Texture1 : public TextureSingle
{
public:
    // Construction.
    Texture1(DFType format, unsigned int length, bool hasMipmaps = false,
        bool createStorage = true);

    // Texture dimensions.
    unsigned int GetLength() const;
};

}
