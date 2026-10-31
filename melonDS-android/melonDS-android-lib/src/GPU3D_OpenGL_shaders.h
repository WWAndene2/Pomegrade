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
};

smooth in vec4 fColor;
smooth in vec2 fTexcoord;
flat in ivec3 fPolygonAttr;
flat in int fHDTexture;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAttr;

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

    int vramOffset = int(fPolygonAttr.y) & 0xFFFF; // bits 16-19: procedural surface (Pomegrade)
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
    ivec2 vramaddr = ivec2((int(fPolygonAttr.y) & 0xFFFF) << 3, int(fPolygonAttr.z >> 16));
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

    int vramOffset = int(fPolygonAttr.y) & 0xFFFF; // bits 16-19: procedural surface (Pomegrade)
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



vec4 FinalColor()
{
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
        vec4 tcol = uTextureFilter != 0 ? TextureLookup_Filtered(fTexcoord) : TextureLookup_Nearest(fTexcoord);
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

    gl_Position = fpos;
}
)";


const char* kRenderFS_ZO = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31.0) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.g = 0.0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1.0;
}
)";

const char* kRenderFS_WO = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31.0) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.g = 0.0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1.0;
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

}

#endif // GPU3D_OPENGL_SHADERS_H
