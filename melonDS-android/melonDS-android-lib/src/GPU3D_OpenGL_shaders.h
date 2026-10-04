/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef GPU3D_OPENGL_SHADERS_H
#define GPU3D_OPENGL_SHADERS_H

#define kShaderHeader "#version 320 es"

namespace melonDS
{
const char* kClearVS = kShaderHeader R"(

in vec2 vPosition;

uniform uint uDepth;

void main()
{
    float fdepth = (float(uDepth) / 8388608.0) - 1.0;
    gl_Position = vec4(vPosition, fdepth, 1.0);
}
)";

const char* kClearFS = kShaderHeader R"(

precision highp float;

uniform uvec4 uColor;
uniform uint uOpaquePolyID;
uniform uint uFogFlag;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAttr;

void main()
{
    oColor = vec4(uColor).bgra / 31.0;
    oAttr.r = float(uOpaquePolyID) / 63.0;
    oAttr.g = 0.0;
    oAttr.b = float(uFogFlag);
    oAttr.a = 1.0;
}
)";



const char* kFinalPassVS = kShaderHeader R"(

in vec2 vPosition;

void main()
{
    // heh
    gl_Position = vec4(vPosition, 0.0, 1.0);
}
)";

const char* kFinalPassEdgeFS = kShaderHeader R"(

precision highp float;

uniform highp sampler2D DepthBuffer;
uniform sampler2D AttrBuffer;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
    int uTextureFilter; // Pomegrade: TextureLookup_Filtered
};

layout(location = 0) out vec4 oColor;

// make up for crapo zbuffer precision
bool isless(float a, float b)
{
    return a < b;

    // a < b
    float diff = a - b;
    return diff < (256.0 / 16777216.0);
}

bool isgood(vec4 attr, float depth, int refPolyID, float refDepth)
{
    int polyid = int(attr.r * 63.0);

    if (polyid != refPolyID && isless(refDepth, depth))
        return true;

    return false;
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    int scale = 1;//int(uScreenSize.x / 256);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    int polyid = int(attr.r * 63.0);

    if (attr.g != 0.0)
    {
        vec4 depthU = texelFetch(DepthBuffer, coord + ivec2(0,-scale), 0);
        vec4 attrU = texelFetch(AttrBuffer, coord + ivec2(0,-scale), 0);
        vec4 depthD = texelFetch(DepthBuffer, coord + ivec2(0,scale), 0);
        vec4 attrD = texelFetch(AttrBuffer, coord + ivec2(0,scale), 0);
        vec4 depthL = texelFetch(DepthBuffer, coord + ivec2(-scale,0), 0);
        vec4 attrL = texelFetch(AttrBuffer, coord + ivec2(-scale,0), 0);
        vec4 depthR = texelFetch(DepthBuffer, coord + ivec2(scale,0), 0);
        vec4 attrR = texelFetch(AttrBuffer, coord + ivec2(scale,0), 0);

        if (isgood(attrU, depthU.r, polyid, depth.r) ||
            isgood(attrD, depthD.r, polyid, depth.r) ||
            isgood(attrL, depthL.r, polyid, depth.r) ||
            isgood(attrR, depthR.r, polyid, depth.r))
        {
            // mark this pixel!

            ret.rgb = uEdgeColors[polyid >> 3].bgr;

            // this isn't quite accurate, but it will have to do
            if ((uDispCnt & (1<<4)) != 0)
                ret.a = 0.5;
            else
                ret.a = 1.0;
        }
    }

    oColor = ret;
}
)";

const char* kFinalPassFogFS = kShaderHeader R"(

precision highp float;
precision highp int;

uniform highp sampler2D DepthBuffer;
uniform sampler2D AttrBuffer;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
    int uTextureFilter; // Pomegrade: TextureLookup_Filtered
};

layout(location = 0) out vec4 oColor;

vec4 CalculateFog(float depth)
{
    int idepth = int(depth * 16777216.0);
    int densityid, densityfrac;

    if (idepth < uFogOffset)
    {
        densityid = 0;
        densityfrac = 0;
    }
    else
    {
        uint udepth = uint(idepth);
        udepth -= uint(uFogOffset);
        udepth = (udepth >> 2) << uint(uFogShift);

        densityid = int(udepth >> 17);
        if (densityid >= 32)
        {
            densityid = 32;
            densityfrac = 0;
        }
        else
            densityfrac = int(udepth & uint(0x1FFFF));
    }

    float density = mix(uFogDensity[densityid], uFogDensity[densityid+1], float(densityfrac)/131072.0);

    return vec4(density, density, density, density);
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    if (attr.b != 0.0) ret = CalculateFog(depth.r);

    oColor = ret;
}
)";



const char* kRenderVSCommon = R"(

precision highp int;
precision highp float;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
    int uTextureFilter; // Pomegrade: TextureLookup_Filtered
    float uRelief;      // Pomegrade: relief depth in texels, 0 = off
    vec4 uReliefLight;  // Pomegrade: towards the main light (view space), w: 1 if known
};

in uvec4 vPosition;
in uvec4 vColor;
in ivec2 vTexcoord;
in ivec3 vPolygonAttr;
in int vHDTexture;

smooth out vec4 fColor;
smooth out vec2 fTexcoord;
flat out ivec3 fPolygonAttr;
flat out int fHDTexture;

// Pomegrade lighting effects: view-space position (w: 1 = perspective, 0 = 2D-like)
// and normal (w: how shiny the material is, 0..1)
in vec4 vViewPosition;
in vec4 vViewNormal;
smooth out vec4 fViewPosition;
smooth out vec4 fViewNormal;
)";

const char* kRenderFSCommon = R"(

precision highp int;
precision highp float;
precision mediump usampler2D;

uniform usampler2D TexMem;
uniform sampler2D TexPalMem;
uniform highp sampler2DArray HDAtlas;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
    int uTextureFilter; // Pomegrade: TextureLookup_Filtered
    float uRelief;      // Pomegrade: relief depth in texels, 0 = off
    vec4 uReliefLight;  // Pomegrade: towards the main light (view space), w: 1 if known
};

smooth in vec4 fColor;
smooth in vec2 fTexcoord;
flat in ivec3 fPolygonAttr;
flat in int fHDTexture;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAttr;

// Pomegrade lighting effects: written by opaque polygons only, when enabled
smooth in vec4 fViewPosition;
smooth in vec4 fViewNormal;
layout(location = 2) out vec4 oViewPosition;
layout(location = 3) out vec4 oViewNormal;

struct ViewData
{
    vec3 Position;
    vec3 Normal;
    float Valid;
    float Specular;
};

// before any discard: the face normal needs screen-space derivatives
ViewData ComputeViewData()
{
    ViewData view;
    view.Position = fViewPosition.xyz;
    // the surface's own orientation: occlusion is about geometry, and the
    // game's smooth normals on flat facets would make each facet occlude itself
    vec3 face = cross(dFdx(fViewPosition.xyz), dFdy(fViewPosition.xyz));
    vec3 normal = dot(face, face) > 0.0 ? normalize(face) : vec3(0.0, 0.0, 1.0);
    // the side seen from the camera (double-sided polygons, normal-less ones)
    if (dot(normal, -view.Position) < 0.0) normal = -normal;
    view.Normal = normal;
    view.Valid = fViewPosition.w > 0.999 ? 1.0 : 0.0;
    view.Specular = clamp(fViewNormal.w, 0.0, 1.0);
    return view;
}

void WriteViewData(ViewData view)
{
    oViewPosition = vec4(view.Position, view.Valid);
    oViewNormal = vec4(view.Normal * 0.5 + 0.5, view.Specular);
}

int TexcoordWrap(int c, int maxc, int mode)
{
    if ((mode & (1<<0)) != 0)
    {
        if ((mode & (1<<2)) != 0 && (c & maxc) != 0)
            return (maxc-1) - (c & (maxc-1));
        else
            return (c & (maxc-1));
    }
    else
        return clamp(c, 0, maxc-1);
}

vec4 TextureFetch_A3I5(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    pixel.a = (pixel.r & 0xE0);
    pixel.a = (pixel.a >> 3) + (pixel.a >> 6);
    pixel.r &= 0x1F;

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, float(pixel.a)/31.0);
}

vec4 TextureFetch_I2(ivec2 addr, ivec4 st, int wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) >> 2;
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    pixel.r >>= (2 * (st.x & 3));
    pixel.r &= 0x03;

    addr.y = (addr.y << 2) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, (pixel.r>0) ? 1.0 : alpha0);
}

vec4 TextureFetch_I4(ivec2 addr, ivec4 st, int wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) >> 1;
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    if ((st.x & 1) != 0) pixel.r >>= 4;
    else                 pixel.r &= 0x0F;

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, (pixel.r>0) ? 1.0 : alpha0);
}

vec4 TextureFetch_I8(ivec2 addr, ivec4 st, int wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, (pixel.r>0) ? 1.0 : alpha0);
}

vec4 TextureFetch_Compressed(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y & 0x3FC) * (st.z>>2)) + (st.x & 0x3FC) + (st.y & 0x3);
    ivec4 p = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    int val = (p.r >> (2 * (st.x & 0x3))) & 0x3;

    int slot1addr = 0x20000 + ((addr.x & 0x1FFFC) >> 1);
    if (addr.x >= 0x40000) slot1addr += 0x10000;

    int palinfo;
    p = ivec4(texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0));
    palinfo = p.r;
    slot1addr++;
    p = ivec4(texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0));
    palinfo |= (p.r << 8);

    addr.y = (addr.y << 3) + ((palinfo & 0x3FFF) << 1);
    palinfo >>= 14;

    if (val == 0)
    {
        vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
        return vec4(color.rgb, 1.0);
    }
    else if (val == 1)
    {
        addr.y++;
        vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
        return vec4(color.rgb, 1.0);
    }
    else if (val == 2)
    {
        if (palinfo == 1)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb + color1.rgb) / 2.0, 1.0);
        }
        else if (palinfo == 3)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb*5.0 + color1.rgb*3.0) / 8.0, 1.0);
        }
        else
        {
            addr.y += 2;
            vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4(color.rgb, 1.0);
        }
    }
    else
    {
        if (palinfo == 2)
        {
            addr.y += 3;
            vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4(color.rgb, 1.0);
        }
        else if (palinfo == 3)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb*3.0 + color1.rgb*5.0) / 8.0, 1.0);
        }
        else
        {
            return vec4(0.0);
        }
    }
}

vec4 TextureFetch_A5I3(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    pixel.a = (pixel.r & 0xF8) >> 3;
    pixel.r &= 0x07;

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, float(pixel.a)/31.0);
}

vec4 TextureFetch_Direct(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) << 1;
    ivec4 pixelL = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    addr.x++;
    ivec4 pixelH = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    vec4 color;
    color.r = float(pixelL.r & 0x1F) / 31.0;
    color.g = float((pixelL.r >> 5) | ((pixelH.r & 0x03) << 3)) / 31.0;
    color.b = float((pixelH.r & 0x7C) >> 2) / 31.0;
    color.a = float(pixelH.r >> 7);

    return color;
}

// HD replacement from the atlas, see GLHDTextures::Lookup for the layout
vec4 TextureLookup_HD(vec2 st)
{
    int attr = int(fPolygonAttr.z << 16);
    int scaleLog2 = (fHDTexture >> 22) & 0x7;
    int tw = (8 << ((attr >> 20) & 0x7)) << scaleLog2;
    int th = (8 << ((attr >> 23) & 0x7)) << scaleLog2;
    int wrapmode = (attr >> 16);

    ivec2 c = ivec2(floor(st * float(1 << scaleLog2)));
    c.x = TexcoordWrap(c.x, tw, wrapmode);
    c.y = TexcoordWrap(c.y, th, wrapmode>>1);

    ivec3 pos = ivec3(((fHDTexture & 0x7F) << 3) + c.x,
                      (((fHDTexture >> 7) & 0x7F) << 3) + c.y,
                      (fHDTexture >> 14) & 0xFF);
    return texelFetch(HDAtlas, pos, 0);
}

vec4 TextureLookup_Nearest(vec2 st)
{
    if (fHDTexture != 0) return TextureLookup_HD(st);

    int vramOffset = int(fPolygonAttr.y);
    int attr = int(fPolygonAttr.z << 16); // Shift just to reuse same code as in original source. Same below
    int paladdr = int(fPolygonAttr.z >> 16);

    float alpha0;
    if ((attr & (1<<29)) != 0) alpha0 = 0.0;
    else                       alpha0 = 1.0;

    int tw = 8 << ((attr >> 20) & 0x7);
    int th = 8 << ((attr >> 23) & 0x7);
    ivec4 st_full = ivec4(ivec2(st), tw, th);

    ivec2 vramaddr = ivec2(vramOffset << 3, paladdr);
    int wrapmode = (attr >> 16);

    int type = (attr >> 26) & 0x7;
    if      (type == 5) return TextureFetch_Compressed(vramaddr, st_full, wrapmode);
    else if (type == 2) return TextureFetch_I2        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 3) return TextureFetch_I4        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 4) return TextureFetch_I8        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 1) return TextureFetch_A3I5      (vramaddr, st_full, wrapmode);
    else if (type == 6) return TextureFetch_A5I3      (vramaddr, st_full, wrapmode);
    else                return TextureFetch_Direct    (vramaddr, st_full, wrapmode);
}

// Pomegrade: one texel of the polygon's texture (the DS's, or its upscaled or
// HD replacement), at integer coordinates, wrapped as the DS wraps
vec4 TexelAt(ivec2 c)
{
    int attr = int(fPolygonAttr.z << 16);
    int wrapmode = (attr >> 16);
    if (fHDTexture != 0)
    {
        int scaleLog2 = (fHDTexture >> 22) & 0x7;
        int tw = (8 << ((attr >> 20) & 0x7)) << scaleLog2;
        int th = (8 << ((attr >> 23) & 0x7)) << scaleLog2;
        c.x = TexcoordWrap(c.x, tw, wrapmode);
        c.y = TexcoordWrap(c.y, th, wrapmode>>1);
        return texelFetch(HDAtlas, ivec3(((fHDTexture & 0x7F) << 3) + c.x,
                                         (((fHDTexture >> 7) & 0x7F) << 3) + c.y,
                                         (fHDTexture >> 14) & 0xFF), 0);
    }

    float alpha0 = ((attr & (1<<29)) != 0) ? 0.0 : 1.0;
    ivec4 st_full = ivec4(c, 8 << ((attr >> 20) & 0x7), 8 << ((attr >> 23) & 0x7));
    ivec2 vramaddr = ivec2(int(fPolygonAttr.y) << 3, int(fPolygonAttr.z >> 16));
    int type = (attr >> 26) & 0x7;
    if      (type == 5) return TextureFetch_Compressed(vramaddr, st_full, wrapmode);
    else if (type == 2) return TextureFetch_I2        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 3) return TextureFetch_I4        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 4) return TextureFetch_I8        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 1) return TextureFetch_A3I5      (vramaddr, st_full, wrapmode);
    else if (type == 6) return TextureFetch_A5I3      (vramaddr, st_full, wrapmode);
    else                return TextureFetch_Direct    (vramaddr, st_full, wrapmode);
}

// Pomegrade texture filtering, without mipmaps. Magnified, a texel keeps its
// colour and only its edges are blended, over one screen pixel ("sharp
// bilinear": no blur, no stair steps); from one texel per pixel it is plain
// bilinear. Where a pixel spans several texels along its footprint's long
// axis (distant and grazing surfaces), up to 4 such samples along that axis
// are averaged (no shimmer, no moire). Colour only: the DS's nearest texel
// keeps deciding alpha, so cut-outs and what is opaque or translucent stay
// exactly the DS's; colour is weighted by alpha (no dark fringes).
vec4 TextureLookup_Filtered(vec2 st)
{
    vec4 nearest = TextureLookup_Nearest(st);
    float scale = fHDTexture != 0 ? float(1 << ((fHDTexture >> 22) & 0x7)) : 1.0;
    vec2 t = st * scale;
    // texels as the nearest lookup picks them, so colour and alpha come from
    // the same texel: it truncates DS texture coordinates (texel 0 spans -1
    // to 1), where the HD path floors them
    if (fHDTexture == 0) t += vec2(lessThan(t, vec2(0.0)));
    t -= 0.5; // texel centres on integers, edges on halves
    vec2 dx = dFdx(t), dy = dFdy(t);
    vec2 span = clamp(abs(dx) + abs(dy), vec2(1.0 / 64.0), vec2(1.0)); // texels per pixel, per axis
    float lx = length(dx), ly = length(dy);
    vec2 axis = lx > ly ? dx : dy;
    int taps = int(clamp(ceil(max(lx, ly)), 1.0, 4.0));

    vec3 rgb = vec3(0.0);
    float weight = 0.0;
    for (int i = 0; i < 4; i++)
    {
        if (i >= taps) break;
        vec2 p = t + axis * ((float(i) + 0.5) / float(taps) - 0.5);
        vec2 base = floor(p);
        vec2 f = clamp((p - base - 0.5) / span + 0.5, 0.0, 1.0);
        ivec2 c = ivec2(base);
        vec4 A = TexelAt(c), B = TexelAt(c + ivec2(1, 0)), C = TexelAt(c + ivec2(0, 1)), D = TexelAt(c + ivec2(1, 1));
        vec4 w = vec4((1.0 - f.x) * (1.0 - f.y) * A.a, f.x * (1.0 - f.y) * B.a, (1.0 - f.x) * f.y * C.a, f.x * f.y * D.a);
        rgb += w.x * A.rgb + w.y * B.rgb + w.z * C.rgb + w.w * D.rgb;
        weight += w.x + w.y + w.z + w.w;
    }
    return vec4(weight > 0.0 ? rgb / weight : nearest.rgb, nearest.a);
}

vec4 TextureLookup_Linear(vec2 texcoord)
{
    ivec2 intpart = ivec2(texcoord);
    vec2 fracpart = fract(texcoord);

    int vramOffset = int(fPolygonAttr.y);
    int attr = int(fPolygonAttr.z << 16);  // Shift just to reuse same code as in original source. Same below
    int paladdr = int(fPolygonAttr.z >> 16);

    float alpha0;
    if ((attr & (1<<29)) != 0) alpha0 = 0.0;
    else                       alpha0 = 1.0;

    int tw = 8 << ((attr >> 20) & 0x7);
    int th = 8 << ((attr >> 23) & 0x7);
    ivec4 st_full = ivec4(intpart, tw, th);

    ivec2 vramaddr = ivec2(vramOffset << 3, paladdr);
    int wrapmode = (attr >> 16);

    vec4 A, B, C, D;
    int type = (attr >> 26) & 0x7;
    if (type == 5)
    {
        A = TextureFetch_Compressed(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_Compressed(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_Compressed(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_Compressed(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else if (type == 2)
    {
        A = TextureFetch_I2(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I2(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I2(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I2(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 3)
    {
        A = TextureFetch_I4(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I4(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I4(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I4(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 4)
    {
        A = TextureFetch_I8(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I8(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I8(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I8(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 1)
    {
        A = TextureFetch_A3I5(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_A3I5(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_A3I5(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_A3I5(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else if (type == 6)
    {
        A = TextureFetch_A5I3(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_A5I3(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_A5I3(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_A5I3(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else
    {
        A = TextureFetch_Direct(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_Direct(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_Direct(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_Direct(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }

    float fx = fracpart.x;
    vec4 AB;
    if (A.a < (0.5/31.0) && B.a < (0.5/31.0))
        AB = vec4(0);
    else
    {
        //if (A.a < (0.5/31.0) || B.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        AB = mix(A, B, fx);
    }

    fx = fracpart.x;
    vec4 CD;
    if (C.a < (0.5/31.0) && D.a < (0.5/31.0))
        CD = vec4(0);
    else
    {
        //if (C.a < (0.5/31.0) || D.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        CD = mix(C, D, fx);
    }

    fx = fracpart.y;
    vec4 ret;
    if (AB.a < (0.5/31.0) && CD.a < (0.5/31.0))
        ret = vec4(0);
    else
    {
        //if (AB.a < (0.5/31.0) || CD.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        ret = mix(AB, CD, fx);
    }

    return ret;
}

// Pomegrade: relief textures (DS_ENGINE_REMAKE.md 14.1, 15.2). The texture's
// brightness is a height (bright = raised); the view ray is marched through it
// (steep parallax, 8 layers) and the surface relit from the height's slope with
// the scene's main DS light. Texture space comes from screen derivatives of
// the view-space position and texture coordinates, so no tangents are needed.
// For opaque, perspective, world-like textures: 4/16 colours, compressed and
// direct colour; 256-colour (characters, in the games looked at) and the
// A3I5/A5I3 effect formats are left flat.
float ReliefLuma(vec2 st)
{
    vec4 c = TextureLookup_Nearest(st);
    return dot(c.rgb, vec3(0.299, 0.587, 0.114));
}

// bilinear between texel centres, over a 2-texel footprint: DS textures are
// noisy at the texel scale, the relief follows their larger shapes
float ReliefHeight(vec2 st)
{
    vec2 p = st * 0.5 - 0.5;
    vec2 f = fract(p), b = (floor(p) + 0.5) * 2.0;
    float h00 = ReliefLuma(b), h10 = ReliefLuma(b + vec2(2.0, 0.0));
    float h01 = ReliefLuma(b + vec2(0.0, 2.0)), h11 = ReliefLuma(b + vec2(2.0, 2.0));
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}

// screen derivatives, taken in uniform control flow (FinalColor's start):
// inside a branch they are undefined (0 on some drivers)
vec3 ReliefDP1, ReliefDP2;
vec2 ReliefDU1, ReliefDU2;

// two pseudo-random numbers in [0, 1) per texel cell (volumetric grass)
vec2 BladeHash(vec2 cell)
{
    vec2 q = vec2(dot(cell, vec2(127.1, 311.7)), dot(cell, vec2(269.5, 183.3)));
    return fract(sin(q) * 43758.5453);
}

// returns the texture coordinate to sample; shade: the relighting factor
vec2 ReliefTexcoord(vec2 st, int textype, out float shade)
{
    shade = 1.0;
    vec3 P = fViewPosition.xyz;
    vec3 dp1 = ReliefDP1, dp2 = ReliefDP2;
    vec2 du1 = ReliefDU1, du2 = ReliefDU2;
    // depth by the texture's material (GLMaterialRelief), eighths of the setting
    float relief = uRelief * float(fPolygonAttr.x & 0xF) / 8.0;
    bool usable = relief > 0.0 && fViewPosition.w > 0.999
        && fColor.a > 0.99 // opaque (the alpha comes with the vertex colour)
        && (textype == 2 || textype == 3 || textype == 5 || textype == 7);
    vec3 N0 = cross(dp1, dp2);
    float det = dot(N0, N0);
    if (!usable || !(det > 0.0)) return st; // view-space units are small: no absolute threshold

    // gradients of the texture coordinates on the surface (texels per unit)
    vec3 dp2p = cross(dp2, N0), dp1p = cross(N0, dp1);
    vec3 gU = (dp2p * du1.x + dp1p * du2.x) / det;
    vec3 gV = (dp2p * du1.y + dp1p * du2.y) / det;
    float gl = length(gU) + length(gV);
    if (!(gl > 0.0)) return st;
    float texel = 2.0 / gl; // one texel, in view-space units

    vec3 N = normalize(N0);
    vec3 V = normalize(-P);
    if (dot(N, V) < 0.0) N = -N;
    float vn = max(dot(N, V), 0.15); // grazing angles: limited
    vec3 Vs = V - N * dot(V, N);
    // texture offset at full depth, in texels (at most 4 x the depth)
    vec2 dir = vec2(dot(Vs, gU), dot(Vs, gV)) * (relief * texel / vn);
    float dl = length(dir);
    if (dl > 4.0 * relief) dir *= 4.0 * relief / dl;

    // Pomegrade: volumetric grass (bit 12, foliage): the slab above the
    // surface holds one blade per texel, a cone of random place and height
    // (taller on brighter texels), coloured by the ground under its root and
    // darker towards it. The view ray is marched down through the slab and
    // the first blade hit is drawn; where none is, the ground deep below.
    // Blades smaller than a pixel would shimmer: plain relief there.
    float footprint = max(length(du1), length(du2)); // texels per pixel
    if ((fPolygonAttr.x & (1<<12)) != 0 && footprint < 0.75)
    {
        const int steps = 12;
        vec2 slab = dir * 2.0; // blades twice the relief depth
        // each pixel starts at a random fraction of a step: no banding (14.2)
        float jitter = BladeHash(floor(gl_FragCoord.xy)).x;
        for (int i = 0; i < steps; i++)
        {
            float h = 1.0 - (float(i) + jitter) / float(steps);
            vec2 p = st - slab * (1.0 - h);
            vec2 cell = floor(p);
            vec2 r = BladeHash(cell);
            vec2 root = cell + 0.2 + 0.6 * r;
            float height = (0.3 + 0.7 * r.y) * (0.5 + 0.5 * ReliefLuma(root));
            float radius = 0.45 * (1.0 - h / height);
            if (h < height && length(p - root) < radius)
            {
                shade = 0.8 + 0.45 * h / height;
                return root;
            }
        }
        shade = 0.75;
        return st - slab;
    }

    const int layers = 8;
    float layer = 1.0 / float(layers);
    vec2 cur = st;
    float depth = 0.0;
    float surface = 1.0 - ReliefHeight(cur);
    float prevSurface = surface;
    for (int i = 0; i < layers && depth < surface; i++)
    {
        prevSurface = surface;
        cur -= dir * layer;
        depth += layer;
        surface = 1.0 - ReliefHeight(cur);
    }
    // between the last two layers
    float after = surface - depth, before = prevSurface - (depth - layer);
    float t = (after - before) != 0.0 ? clamp(after / (after - before), 0.0, 1.0) : 0.0;
    cur += dir * layer * t;

    // relighting: the slope of the height under the main light
    float hx = ReliefHeight(cur + vec2(1.0, 0.0)) - ReliefHeight(cur - vec2(1.0, 0.0));
    float hy = ReliefHeight(cur + vec2(0.0, 1.0)) - ReliefHeight(cur - vec2(0.0, 1.0));
    vec3 tU = normalize(gU), tV = normalize(gV);
    // slope: height change per texel times the relief depth in texels
    vec3 Np = normalize(N - (tU * hx + tV * hy) * (1.5 * relief));
    vec3 L = uReliefLight.w > 0.5 ? normalize(uReliefLight.xyz) : normalize(V + vec3(0.0, 1.0, 0.0));
    float base = max(dot(N, L), 0.0), lit = max(dot(Np, L), 0.0);
    shade = clamp((0.35 + lit) / (0.35 + base), 0.5, 1.6);
    return cur;
}

vec4 FinalColor()
{
    ReliefDP1 = dFdx(fViewPosition.xyz);
    ReliefDP2 = dFdy(fViewPosition.xyz);
    ReliefDU1 = dFdx(fTexcoord);
    ReliefDU2 = dFdy(fTexcoord);
    vec4 col;
    vec4 vcol = fColor;
    int blendmode = (fPolygonAttr.x >> 4) & 0x3;

    if (blendmode == 2)
    {
        if ((uDispCnt & (1<<1)) == 0)
        {
            // toon
            vec3 tooncolor = uToonColors[int(vcol.r * 31.0)].rgb;
            vcol.rgb = tooncolor;
        }
        else
        {
            // highlight
            vcol.rgb = vcol.rrr;
        }
    }

    if ((((fPolygonAttr.z >> 10) & 0x7) == 0) || ((uDispCnt & (1<<0)) == 0))
    {
        // no texture
        col = vcol;
    }
    else
    {
        float reliefShade;
        vec2 st = ReliefTexcoord(fTexcoord, (fPolygonAttr.z >> 10) & 0x7, reliefShade);
        vec4 tcol = uTextureFilter != 0 ? TextureLookup_Filtered(st) : TextureLookup_Nearest(st);
        tcol.rgb = min(tcol.rgb * reliefShade, 1.0);
        //vec4 tcol = TextureLookup_Linear(fTexcoord);

        if (fHDTexture == 0)
        {
            // Pomegrade: a DS texel, blended with the DS's integer formulas
            // (GPU3D_Soft.cpp): 6-bit channels, a 5-bit texel widened to 2t+1
            // (0 stays 0), the vertex colour's 9 bits cut to 6. In floats
            // the result was up to a level darker (texel 5: 41/255 here,
            // 11/63 on the DS). A filtered texel, between two DS texels, is
            // widened the same way without being rounded to one of them
            vec3 t5 = uTextureFilter != 0 ? tcol.rgb * 31.0 : floor(tcol.rgb * 31.0 + 0.5);
            vec3 t6 = t5 * 2.0 + min(t5, vec3(1.0));
            vec3 v6 = floor((floor(vcol.rgb * 255.0 + 0.5)) / 4.0);
            float ta = uTextureFilter != 0 ? tcol.a * 31.0 : floor(tcol.a * 31.0 + 0.5), va = floor(vcol.a * 31.0 + 0.5);
            if ((blendmode & 1) != 0)
            {
                // decal
                col.rgb = ta == 0.0 ? v6 : ta == 31.0 ? t6 : floor((t6 * ta + v6 * (31.0 - ta)) / 32.0);
                col.rgb /= 63.0;
                col.a = vcol.a;
            }
            else
            {
                // modulate
                col.rgb = floor(((t6 + 1.0) * (v6 + 1.0) - 1.0) / 64.0) / 63.0;
                col.a = floor(((ta + 1.0) * (va + 1.0) - 1.0) / 32.0) / 31.0;
            }
        }
        else if ((blendmode & 1) != 0)
        {
            // decal
            col.rgb = (tcol.rgb * tcol.a) + (vcol.rgb * (1.0-tcol.a));
            col.a = vcol.a;
        }
        else
        {
            // modulate
            col = vcol * tcol;
        }
    }

    if (blendmode == 2)
    {
        if ((uDispCnt & (1<<1)) != 0)
        {
            vec3 tooncolor = uToonColors[int(vcol.r * 31.0)].rgb;
            col.rgb = min(col.rgb + tooncolor, 1.0);
        }
    }

    return col.bgra;
}
)";


const char* kRenderVS_Z = R"(

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = (((vec2(vPosition.xy) ) * 2.0) / uScreenSize) - 1.0;
    fpos.z = (float(vPosition.z << zshift) / 8388608.0) - 1.0;
    // Pomegrade: "depth test equal" polygons (decals) pass within the DS margin
    // of +-0x200 (Z-buffer): moved that much towards the camera, then LEQUAL
    if ((attr & (1<<14)) != 0) fpos.z -= 512.0 / 8388608.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xyz *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;
    fHDTexture = vHDTexture;
    fViewPosition = vViewPosition;
    fViewNormal = vViewNormal;

    gl_Position = fpos;
}
)";

const char* kRenderVS_W = R"(

smooth out float fZ;

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = (((vec2(vPosition.xy) ) * 2.0) / uScreenSize) - 1.0;
    fpos.z = 0.0;
    fZ = float(vPosition.z << zshift) / 16777216.0;
    // Pomegrade: "depth test equal" polygons (decals) pass within the DS margin
    // of +-0xFF (W-buffer): moved that much towards the camera, then LEQUAL
    if ((attr & (1<<14)) != 0) fZ -= 255.0 / 16777216.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xy *= fpos.w;
    fpos.z = 0.0;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;
    fHDTexture = vHDTexture;
    fViewPosition = vViewPosition;
    fViewNormal = vViewNormal;

    gl_Position = fpos;
}
)";


const char* kRenderFS_ZO = R"(

void main()
{
    ViewData view = ComputeViewData();
    vec4 col = FinalColor();
    if (col.a < 30.5/31.0) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.g = 0.0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1.0;
    WriteViewData(view);
}
)";

const char* kRenderFS_WO = R"(

smooth in float fZ;

void main()
{
    ViewData view = ComputeViewData();
    vec4 col = FinalColor();
    if (col.a < 30.5/31.0) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.g = 0.0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1.0;
    WriteViewData(view);
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZE = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31.0) discard;

    oAttr.g = 1.0;
    oAttr.a = 1.0;
}
)";

const char* kRenderFS_WE = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31.0) discard;

    oAttr.g = 1.0;
    oAttr.a = 1.0;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZT = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31.0) discard;
    if (col.a >= 30.5/31.0) discard;

    oColor = col;
    oAttr.b = 0.0;
    oAttr.a = 1.0;
}
)";

const char* kRenderFS_WT = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31.0) discard;
    if (col.a >= 30.5/31.0) discard;

    oColor = col;
    oAttr.b = 0.0;
    oAttr.a = 1.0;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZSM = R"(

void main()
{
    oColor = vec4(0,0,0,1);
}
)";

const char* kRenderFS_WSM = R"(

smooth in float fZ;

void main()
{
    oColor = vec4(0,0,0,1);
    gl_FragDepth = fZ;
}
)";

// Pomegrade lighting effects, after the frame is rendered (see GLRenderer::RenderLighting).
// Inputs: the view-space position and normal of the opaque pixels.

// Ambient occlusion, screen-space (McGuire, Mara, Luebke, "Scalable Ambient
// Obscurance", HPG 2012): around each pixel, the neighbours that rise above
// its surface hide part of the sky, the near ones more.
// Light bounce, from the same samples: neighbours in front of the surface and
// facing it send back part of their light, with their colour (one bounce,
// diffuse, from what the screen shows).
// Out 0: R = visibility (1 = open), G = size of one pixel in view units here
// (for the filter pass). Out 1: light received from the neighbours.
const char* kLightingAOFS = kShaderHeader R"(

precision highp float;
precision highp int;

uniform highp sampler2D GPosition;
uniform sampler2D GNormal;
uniform sampler2D Color;
uniform float uRadius;       // ambient occlusion, in output pixels
uniform float uBounceRadius; // light bounce, in output pixels (light travels further than shadowing matters)
uniform bool uPixelInW;      // on the smaller copy: GPosition.w is 1 + the size of one of its pixels (kLightingDownsampleFS)

layout(location = 0) out vec4 oAO;
layout(location = 1) out vec4 oBounce;

const int NumSamples = 12;
// moderate: DS games often have shading painted into their textures already.
// Test scene (tests/lighting-effects): floor-wall crease 19% darker, contact
// shadow 15% darker, open surfaces unchanged
const float Intensity = 2.0;

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(GPosition, 0);
    vec4 gp = texelFetch(GPosition, p, 0);
    if (gp.w < 0.5) { oAO = vec4(1.0, 0.0, 0.0, 1.0); oBounce = vec4(0.0); return; }

    vec3 P = gp.xyz;
    vec3 N = texelFetch(GNormal, p, 0).xyz * 2.0 - 1.0;

    // one pixel's size on this surface: the closest neighbour (the others may
    // be across a silhouette)
    float pixel = 1e30;
    const ivec2 dirs[4] = ivec2[4](ivec2(1, 0), ivec2(-1, 0), ivec2(0, 1), ivec2(0, -1));
    if (uPixelInW)
        pixel = gp.w - 1.0;
    else
    for (int i = 0; i < 4; i++)
    {
        vec4 q = texelFetch(GPosition, clamp(p + dirs[i], ivec2(0), size - 1), 0);
        if (q.w > 0.5 && q.xyz != P) pixel = min(pixel, length(q.xyz - P));
    }
    if (pixel > 1e29) { oAO = vec4(1.0, 0.0, 0.0, 1.0); oBounce = vec4(0.0); return; }

    float radius = pixel * uRadius; // in view units
    // 16 rotations of the sample spiral over 4x4 pixels, averaged by the filter pass
    int cell = (p.x & 3) + ((p.y & 3) << 2);
    float rotation = float(cell) * (6.2831853 / 16.0);

    float occlusion = 0.0;
    vec3 bounce = vec3(0.0);
    for (int i = 0; i < NumSamples; i++)
    {
        float t = (float(i) + 0.5) / float(NumSamples);
        float angle = float(i) * 2.3999632 + rotation; // golden angle spiral
        vec2 offset = vec2(cos(angle), sin(angle)) * (t * uRadius + 1.0);
        ivec2 sp = clamp(p + ivec2(round(offset)), ivec2(0), size - 1);
        vec4 q = texelFetch(GPosition, sp, 0);
        if (q.w < 0.5) continue;

        vec3 v = q.xyz - P;
        float vv = dot(v, v);
        // SAO's estimator, made scale-free with the radius: occluders above the
        // surface count more the closer they are, nothing past the radius
        float f = max(1.0 - vv / (radius * radius), 0.0);
        float vn = dot(v, N) - 0.02 * radius; // bias against self-occlusion
        occlusion += f * f * f * max(vn, 0.0) * radius / (vv + 0.01 * radius * radius);
    }

    // light bounce: same spiral, further out. Cosines at both ends (the sample
    // faces this surface, which faces the sample), smooth falloff to the radius
    float bounceRadius = pixel * uBounceRadius;
    for (int i = 0; i < NumSamples; i++)
    {
        float t = (float(i) + 0.5) / float(NumSamples);
        float angle = float(i) * 2.3999632 + rotation;
        vec2 offset = vec2(cos(angle), sin(angle)) * (t * uBounceRadius + 1.0);
        ivec2 sp = clamp(p + ivec2(round(offset)), ivec2(0), size - 1);
        vec4 q = texelFetch(GPosition, sp, 0);
        if (q.w < 0.5) continue;

        vec3 v = q.xyz - P;
        float vv = dot(v, v);
        if (vv <= 0.0) continue;
        vec3 nq = texelFetch(GNormal, sp, 0).xyz * 2.0 - 1.0;
        float d = sqrt(vv);
        float cosHere = dot(v, N) / d, cosThere = -dot(v, nq) / d;
        float f = max(1.0 - vv / (bounceRadius * bounceRadius), 0.0);
        if (cosHere > 0.0 && cosThere > 0.0)
            bounce += texelFetch(Color, sp, 0).rgb * (cosHere * cosThere * f * f);
    }

    float visibility = clamp(1.0 - Intensity * occlusion / float(NumSamples), 0.0, 1.0);
    oAO = vec4(visibility, pixel, 0.0, 1.0);
    oBounce = vec4(bounce / float(NumSamples), 1.0);
}
)";

// Shadow map: depth of the opaque 3D geometry as seen from the main light,
// orthographic, fitted to the scene (see GLRenderer::RenderShadowMap).
const char* kLightingShadowVS = kShaderHeader R"(

precision highp float;

in vec4 vViewPosition; // xyz, w: 1 = perspective (2D-like geometry casts nothing)

uniform vec3 uLightRight, uLightUp, uLightDir; // light space, uLightDir towards the light
uniform vec4 uShadowBounds; // min x, min y, 1 / width, 1 / height
uniform vec2 uShadowDepth;  // nearest to the light, 1 / depth range

void main()
{
    vec3 p = vViewPosition.xyz;
    vec2 uv = (vec2(dot(p, uLightRight), dot(p, uLightUp)) - uShadowBounds.xy) * uShadowBounds.zw;
    float depth = (uShadowDepth.x - dot(p, uLightDir)) * uShadowDepth.y;
    gl_Position = vViewPosition.w > 0.5 ? vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0) : vec4(2.0, 2.0, 2.0, 1.0);
}
)";

const char* kLightingShadowFS = kShaderHeader R"(

precision highp float;

void main()
{
}
)";

// Applies the lighting terms to the rendered frame, filtering them over the
// 4x4 rotation pattern within the same surface. The pixel's own colour stands
// for its albedo: the light it receives from a bounce is tinted by it.
// Shared by the passes that compute the lighting terms: inputs, the shadow
// and reflection functions.
#define kLightingCommon R"(

precision highp float;
precision highp int;

uniform sampler2D Color;
uniform highp sampler2D GPosition;
uniform sampler2D GNormal;
uniform highp sampler2D AO;
uniform highp sampler2D Bounce;
uniform bool uAmbientOcclusion;
uniform float uBounceIntensity; // 0 = off

// shadows from the main light
uniform highp sampler2DShadow ShadowMap;
uniform highp sampler2D ShadowDepthMap; // the same map, its depths as stored (no comparison)
uniform float uShadowStrength; // 0 = off
uniform vec3 uLightRight, uLightUp, uLightDir;
uniform vec4 uShadowBounds;
uniform vec2 uShadowDepth;
uniform float uShadowTexel; // one shadow map texel, in view units

// reflections, traced on screen
uniform float uReflectionStrength; // 0 = off
uniform mat4 uProj;                // the frame's perspective projection (DS matrix as is)
uniform vec4 uViewport;            // DS viewport: x0, y1 (top), width, height, in native pixels
uniform float uScale;              // output pixels per native pixel

// output pixel of a view-space point, as the DS viewport transform places it
vec2 ScreenOf(vec3 view)
{
    vec4 clip = uProj * vec4(view, 4096.0);
    return vec2((clip.x + clip.w) * uViewport.z / (2.0 * clip.w) + uViewport.x,
                (-clip.y + clip.w) * uViewport.w / (2.0 * clip.w) + uViewport.y) * uScale;
}

// Colour reflected at P (view space, camera at the origin) off a surface of
// normal N, w = how sure the hit is (0 = nothing found on screen).
vec4 Reflection(vec3 P, vec3 N, float pixel, ivec2 size)
{
    vec3 dir = reflect(normalize(P), N);
    float step = pixel * 2.0;
    float t = step;
    vec3 prev = P;
    const int Steps = 48;
    for (int i = 0; i < Steps; i++)
    {
        vec3 R = P + dir * t;
        vec2 s = ScreenOf(R);
        if (any(lessThan(s, vec2(0.0))) || any(greaterThanEqual(s, vec2(size)))) break;
        vec4 g = texelFetch(GPosition, ivec2(s), 0);
        // the ray went behind a surface on screen: find where it crossed it,
        // a hit if it really meets the surface there (rather than passing
        // behind an object)
        if (g.w > 0.5 && length(R) > length(g.xyz))
        {
            vec3 a = prev, b = R;
            for (int k = 0; k < 8; k++)
            {
                vec3 m = (a + b) * 0.5;
                vec4 gm = texelFetch(GPosition, ivec2(ScreenOf(m)), 0);
                if (gm.w > 0.5 && length(m) > length(gm.xyz)) b = m; else a = m;
            }
            vec2 hit = ScreenOf(b);
            vec4 gh = texelFetch(GPosition, ivec2(hit), 0);
            // a reflection only sees surfaces facing the ray; one facing away
            // is the reflecting surface itself, crossed by the ray's first
            // steps where it is seen at a grazing angle (one pixel there
            // spans several pixels of depth): march on
            vec3 nh = texelFetch(GNormal, ivec2(hit), 0).xyz * 2.0 - 1.0;
            if (gh.w > 0.5 && dot(nh, dir) > -0.05)
            {
                prev = R;
                step *= 1.08;
                t += step;
                continue;
            }
            if (gh.w < 0.5 || length(b) - length(gh.xyz) > max(step, pixel * 4.0)) return vec4(0.0);
            // fade near the screen's edges and towards the end of the march
            vec2 edge = min(hit, vec2(size) - hit) / (vec2(size) * 0.1);
            float sure = clamp(min(edge.x, edge.y), 0.0, 1.0) * (1.0 - float(i) / float(Steps));
            return vec4(texelFetch(Color, ivec2(hit), 0).rgb, sure);
        }
        prev = R;
        step *= 1.08;
        t += step;
    }
    return vec4(0.0);
}

// 16 points spread evenly over the unit disc (Vogel spiral)
vec2 DiscPoint(int i, float rotation)
{
    float r = sqrt((float(i) + 0.5) / 16.0);
    float a = float(i) * 2.39996323 + rotation;
    return r * vec2(cos(a), sin(a));
}

// light's apparent size: penumbra width per unit of distance between the
// caster and the receiver (tan of the light's angular radius, ~1 degree:
// at ~3 degrees the long shadows of a low light washed out entirely)
const float PenumbraSlope = 0.02;
// widest penumbra searched, in shadow map texels
const float MaxPenumbra = 12.0;

// 1 = lit by the main light, 0 = in its shadow. Contact-hardening (PCSS):
// sharp where a shadow meets its caster, softer the further it falls
float ShadowLit(vec3 P, vec3 N)
{
    vec2 uv = (vec2(dot(P, uLightRight), dot(P, uLightUp)) - uShadowBounds.xy) * uShadowBounds.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 1.0;
    float cosLight = max(dot(N, uLightDir), 0.05);
    // against self-shadowing: a couple of texels, more on surfaces the light grazes
    float bias = uShadowTexel * (1.5 + 1.0 / cosLight) * uShadowDepth.y;
    float depth = (uShadowDepth.x - dot(P, uLightDir)) * uShadowDepth.y - bias;
    vec2 texel = 1.0 / vec2(textureSize(ShadowMap, 0));
    // a different rotation of the disc per pixel: noise instead of banding
    float rotation = 6.2831853 * fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));

    // 1. the casters: average depth of what is between this point and the light
    float blockers = 0.0, count = 0.0;
    for (int i = 0; i < 16; i++)
    {
        float d = texture(ShadowDepthMap, uv + DiscPoint(i, rotation) * MaxPenumbra * texel).r;
        if (d < depth) { blockers += d; count += 1.0; }
    }
    if (count == 0.0) return 1.0;
    float distance = (depth - blockers / count) / uShadowDepth.y; // view units
    // 2. the penumbra from the caster's distance, filtered over that width
    float radius = clamp(distance * PenumbraSlope / uShadowTexel, 1.0, MaxPenumbra);
    float lit = 0.0;
    for (int i = 0; i < 16; i++)
        lit += texture(ShadowMap, vec3(uv + DiscPoint(i, rotation) * radius * texel, depth));
    return lit / 16.0;
}

)"

const char* kLightingComposeFS = kShaderHeader kLightingCommon R"(layout(location = 0) out vec4 oColor;

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(GPosition, 0);
    vec4 col = texelFetch(Color, p, 0);
    vec4 gp = texelFetch(GPosition, p, 0);
    vec2 ao = texelFetch(AO, p, 0).rg;
    if (gp.w < 0.5 || ao.g <= 0.0) { oColor = col; return; }

    vec3 P = gp.xyz;
    vec3 N = texelFetch(GNormal, p, 0).xyz * 2.0 - 1.0;
    float pixel = ao.g;

    float sum = 0.0, weight = 0.0;
    vec3 bounce = vec3(0.0);
    for (int y = -2; y < 2; y++)
    {
        for (int x = -2; x < 2; x++)
        {
            ivec2 sp = clamp(p + ivec2(x, y), ivec2(0), size - 1);
            vec4 q = texelFetch(GPosition, sp, 0);
            if (q.w < 0.5) continue;
            vec3 nq = texelFetch(GNormal, sp, 0).xyz * 2.0 - 1.0;
            // same surface: similar normal, near the plane
            float plane = abs(dot(q.xyz - P, N));
            if (dot(nq, N) < 0.8 || plane > pixel * 2.0) continue;
            sum += texelFetch(AO, sp, 0).r;
            bounce += texelFetch(Bounce, sp, 0).rgb;
            weight += 1.0;
        }
    }
    float visibility = weight > 0.0 ? sum / weight : ao.r;
    bounce = weight > 0.0 ? bounce / weight : texelFetch(Bounce, p, 0).rgb;

    vec3 lit = col.rgb * (uAmbientOcclusion ? visibility : 1.0);
    if (uShadowStrength > 0.0)
    {
        // the share of the colour the main light brought (its cosine on this
        // surface) goes in its shadow; surfaces facing away keep their colour
        float cosLight = max(dot(N, uLightDir), 0.0);
        lit *= 1.0 - uShadowStrength * cosLight * (1.0 - ShadowLit(P, N));
    }
    lit += col.rgb * bounce * uBounceIntensity;
    if (uReflectionStrength > 0.0)
    {
        // Fresnel (Schlick): every surface reflects at grazing angles, shiny
        // materials (the game's specular colour) more and head-on too
        float specular = texelFetch(GNormal, p, 0).w;
        float f0 = 0.02 + 0.4 * specular;
        float cosView = clamp(dot(-normalize(P), N), 0.0, 1.0);
        float fresnel = f0 + (1.0 - f0) * pow(1.0 - cosView, 5.0);
        vec4 reflected = Reflection(P, N, pixel, size);
        float amount = uReflectionStrength * fresnel * (0.25 + 0.75 * specular) * reflected.w;
        lit = mix(lit, reflected.rgb, amount);
    }
    oColor = vec4(min(lit, vec3(1.0)), col.a);
}
)";


// At high resolutions the lighting terms are computed on a smaller copy of
// the frame (GLRenderer::LightingFactor times smaller each way), then brought
// back to full resolution by an edge-aware filter: occlusion, light bounce,
// shadows and reflections change slowly across a surface, while the
// full-resolution pass keeps what is sharp (the colours, each pixel's own
// normal for the light's angle, Fresnel and shininess).

// The smaller copy: per block of pixels one sample, alternately the one
// nearest to the camera and the one farthest, so both sides of a silhouette
// have samples nearby for the filter.
const char* kLightingDownsampleFS = kShaderHeader R"(

precision highp float;
precision highp int;

uniform highp sampler2D GPosition;
uniform sampler2D GNormal;
uniform sampler2D Color;
uniform int uFactor;

layout(location = 0) out vec4 oPosition;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oColor;

void main()
{
    ivec2 lp = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(GPosition, 0);
    ivec2 base = lp * uFactor;
    bool nearest = ((lp.x + lp.y) & 1) == 0;
    ivec2 best = ivec2(-1);
    float bestDistance = 0.0;
    for (int y = 0; y < uFactor; y++)
    {
        for (int x = 0; x < uFactor; x++)
        {
            ivec2 q = min(base + ivec2(x, y), size - 1);
            vec4 g = texelFetch(GPosition, q, 0);
            if (g.w < 0.5) continue;
            float d = length(g.xyz);
            if (best.x < 0 || (nearest ? d < bestDistance : d > bestDistance)) { best = q; bestDistance = d; }
        }
    }
    if (best.x < 0) best = min(base, size - 1);
    oPosition = texelFetch(GPosition, best, 0);
    oNormal = texelFetch(GNormal, best, 0);
    oColor = texelFetch(Color, best, 0);
    if (oPosition.w > 0.5)
    {
        // the size of one pixel of this copy on the surface, for the effects'
        // radii: the closest full-resolution neighbour, uFactor times (the
        // samples kept from neighbouring blocks are unevenly spaced). In w,
        // after the 1 marking data; 1e30 = unknown, as in kLightingAOFS
        vec3 P = oPosition.xyz;
        float pixel = 1e30;
        const ivec2 dirs[4] = ivec2[4](ivec2(1, 0), ivec2(-1, 0), ivec2(0, 1), ivec2(0, -1));
        for (int i = 0; i < 4; i++)
        {
            vec4 q = texelFetch(GPosition, clamp(best + dirs[i], ivec2(0), size - 1), 0);
            if (q.w > 0.5 && q.xyz != P) pixel = min(pixel, length(q.xyz - P));
        }
        oPosition.w = 1.0 + (pixel > 1e29 ? 1e30 : pixel * float(uFactor));
    }
}
)";

// The terms on the smaller copy: what kLightingComposeFS computes before
// applying it. Out 0: visibility (ambient occlusion, filtered), share lit by
// the main light, how sure the reflection is, 1 = has data. Out 1: light
// received from the neighbours (filtered). Out 2: the reflected colour.
const char* kLightingTermsFS = kShaderHeader kLightingCommon R"(
layout(location = 0) out vec4 oTerms;
layout(location = 1) out vec4 oBounce;
layout(location = 2) out vec4 oReflected;

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(GPosition, 0);
    vec4 gp = texelFetch(GPosition, p, 0);
    vec2 ao = texelFetch(AO, p, 0).rg;
    if (gp.w < 0.5 || ao.g <= 0.0) { oTerms = vec4(1.0, 1.0, 0.0, 0.0); oBounce = vec4(0.0); oReflected = vec4(0.0); return; }

    vec3 P = gp.xyz;
    vec3 N = texelFetch(GNormal, p, 0).xyz * 2.0 - 1.0;
    float pixel = ao.g;

    float sum = 0.0, weight = 0.0;
    vec3 bounce = vec3(0.0);
    for (int y = -2; y < 2; y++)
    {
        for (int x = -2; x < 2; x++)
        {
            ivec2 sp = clamp(p + ivec2(x, y), ivec2(0), size - 1);
            vec4 q = texelFetch(GPosition, sp, 0);
            if (q.w < 0.5) continue;
            vec3 nq = texelFetch(GNormal, sp, 0).xyz * 2.0 - 1.0;
            float plane = abs(dot(q.xyz - P, N));
            if (dot(nq, N) < 0.8 || plane > pixel * 2.0) continue;
            sum += texelFetch(AO, sp, 0).r;
            bounce += texelFetch(Bounce, sp, 0).rgb;
            weight += 1.0;
        }
    }
    float visibility = weight > 0.0 ? sum / weight : ao.r;
    bounce = weight > 0.0 ? bounce / weight : texelFetch(Bounce, p, 0).rgb;

    float lit = uShadowStrength > 0.0 ? ShadowLit(P, N) : 1.0;
    vec4 reflected = uReflectionStrength > 0.0 ? Reflection(P, N, pixel, size) : vec4(0.0);
    oTerms = vec4(visibility, lit, reflected.w, 1.0);
    oBounce = vec4(bounce, 1.0);
    oReflected = vec4(reflected.rgb, 1.0);
}
)";

// Back to full resolution: each pixel takes the terms of the nearby samples
// of the smaller copy on its own surface (similar normal, near its plane),
// weighted by distance; if none, the closest such sample a little further;
// if none either, no effect. Then applied as in kLightingComposeFS.
const char* kLightingUpsampleFS = kShaderHeader R"(

precision highp float;
precision highp int;

uniform sampler2D Color;
uniform highp sampler2D GPosition;
uniform sampler2D GNormal;
uniform highp sampler2D LowPosition;
uniform sampler2D LowNormal;
uniform sampler2D Terms;
uniform highp sampler2D TermsBounce;
uniform sampler2D TermsReflected;
uniform highp sampler2D LowAO; // its G: one pixel of the smaller copy, in view units
uniform int uFactor;
uniform bool uAmbientOcclusion;
uniform float uBounceIntensity;
uniform float uShadowStrength;
uniform vec3 uLightDir;
uniform float uReflectionStrength;

layout(location = 0) out vec4 oColor;

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec4 col = texelFetch(Color, p, 0);
    vec4 gp = texelFetch(GPosition, p, 0);
    if (gp.w < 0.5) { oColor = col; return; }
    vec3 P = gp.xyz;
    vec4 gn = texelFetch(GNormal, p, 0);
    vec3 N = gn.xyz * 2.0 - 1.0;

    ivec2 lowSize = textureSize(LowPosition, 0);
    vec2 c = (vec2(p) + 0.5) / float(uFactor) - 0.5;
    ivec2 base = ivec2(floor(c));
    vec2 f = c - vec2(base);

    float weight = 0.0, visibility = 0.0, lit = 0.0, sure = 0.0;
    vec3 bounce = vec3(0.0), reflected = vec3(0.0);
    for (int y = 0; y < 2; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            ivec2 q = clamp(base + ivec2(x, y), ivec2(0), lowSize - 1);
            vec4 t = texelFetch(Terms, q, 0);
            if (t.w < 0.5) continue;
            vec3 Q = texelFetch(LowPosition, q, 0).xyz;
            vec3 nq = texelFetch(LowNormal, q, 0).xyz * 2.0 - 1.0;
            float pixel = texelFetch(LowAO, q, 0).g;
            if (dot(nq, N) < 0.8 || abs(dot(Q - P, N)) > pixel * 2.0) continue;
            float w = (x == 1 ? f.x : 1.0 - f.x) * (y == 1 ? f.y : 1.0 - f.y) + 1e-3;
            weight += w;
            visibility += w * t.r;
            lit += w * t.g;
            sure += w * t.b;
            bounce += w * texelFetch(TermsBounce, q, 0).rgb;
            reflected += w * t.b * texelFetch(TermsReflected, q, 0).rgb;
        }
    }
    if (weight == 0.0)
    {
        // a pixel whose surface the four nearest samples don't show (near a
        // silhouette): the closest sample of its surface a little further
        float bestPlane = 1e30;
        for (int y = -1; y < 3; y++)
        {
            for (int x = -1; x < 3; x++)
            {
                ivec2 q = clamp(base + ivec2(x, y), ivec2(0), lowSize - 1);
                vec4 t = texelFetch(Terms, q, 0);
                if (t.w < 0.5) continue;
                vec3 Q = texelFetch(LowPosition, q, 0).xyz;
                vec3 nq = texelFetch(LowNormal, q, 0).xyz * 2.0 - 1.0;
                float pixel = texelFetch(LowAO, q, 0).g;
                float plane = abs(dot(Q - P, N));
                if (dot(nq, N) < 0.8 || plane > pixel * 4.0 || plane >= bestPlane) continue;
                bestPlane = plane;
                weight = 1.0;
                visibility = t.r;
                lit = t.g;
                sure = t.b;
                bounce = texelFetch(TermsBounce, q, 0).rgb;
                reflected = t.b * texelFetch(TermsReflected, q, 0).rgb;
            }
        }
    }
    if (weight == 0.0) { oColor = col; return; }
    reflected = sure > 0.0 ? reflected / sure : vec3(0.0);
    visibility /= weight;
    lit /= weight;
    sure /= weight;
    bounce /= weight;

    vec3 result = col.rgb * (uAmbientOcclusion ? visibility : 1.0);
    if (uShadowStrength > 0.0)
    {
        float cosLight = max(dot(N, uLightDir), 0.0);
        result *= 1.0 - uShadowStrength * cosLight * (1.0 - lit);
    }
    result += col.rgb * bounce * uBounceIntensity;
    if (uReflectionStrength > 0.0)
    {
        float specular = gn.w;
        float f0 = 0.02 + 0.4 * specular;
        float cosView = clamp(dot(-normalize(P), N), 0.0, 1.0);
        float fresnel = f0 + (1.0 - f0) * pow(1.0 - cosView, 5.0);
        float amount = uReflectionStrength * fresnel * (0.25 + 0.75 * specular) * sure;
        result = mix(result, reflected, amount);
    }
    oColor = vec4(min(result, vec3(1.0)), col.a);
}
)";
}
#endif // GPU3D_OPENGL_SHADERS_H
