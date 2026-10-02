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
        vec4 tcol = TextureLookup_Nearest(fTexcoord);
        //vec4 tcol = TextureLookup_Linear(fTexcoord);

        if ((blendmode & 1) != 0)
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
const char* kLightingComposeFS = kShaderHeader R"(

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

// 1 = lit by the main light, 0 = in its shadow (3x3 filtered)
float ShadowLit(vec3 P, vec3 N)
{
    vec2 uv = (vec2(dot(P, uLightRight), dot(P, uLightUp)) - uShadowBounds.xy) * uShadowBounds.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 1.0;
    float cosLight = max(dot(N, uLightDir), 0.05);
    // against self-shadowing: a couple of texels, more on surfaces the light grazes
    float bias = uShadowTexel * (1.5 + 1.0 / cosLight) * uShadowDepth.y;
    float depth = (uShadowDepth.x - dot(P, uLightDir)) * uShadowDepth.y - bias;
    vec2 texel = 1.0 / vec2(textureSize(ShadowMap, 0));
    float lit = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            lit += texture(ShadowMap, vec3(uv + vec2(x, y) * texel, depth));
    return lit / 9.0;
}

layout(location = 0) out vec4 oColor;

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
}
#endif // GPU3D_OPENGL_SHADERS_H
