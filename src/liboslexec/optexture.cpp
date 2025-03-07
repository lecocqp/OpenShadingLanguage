// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage


/////////////////////////////////////////////////////////////////////////
/// \file
///
/// Shader interpreter implementation of texture operations.
///
/////////////////////////////////////////////////////////////////////////

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/simd.h>

#include <cmath>
#include <iostream>

#include "oslexec_pvt.h"
#include <OSL/dual.h>


OSL_NAMESPACE_ENTER
namespace pvt {


// Utility: retrieve a pointer to the ShadingContext's texture options
// struct, also re-initialize its contents.
OSL_SHADEOP void*
osl_get_texture_options(void* sg_)
{
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    TextureOpt* opt   = sg->context->texture_options_ptr();
    new (opt) TextureOpt;
    return opt;
}


OSL_SHADEOP void
osl_texture_set_firstchannel(void* opt, int x)
{
    ((TextureOpt*)opt)->firstchannel = x;
}

OSL_SHADEOP int
osl_texture_decode_wrapmode(ustring_pod name)
{
    return OIIO::TextureOpt::decode_wrapmode(ustring_from(USTR(name)));
}

OSL_SHADEOP void
osl_texture_set_swrap(void* opt, ustring_pod x)
{
    ((TextureOpt*)opt)->swrap = TextureOpt::decode_wrapmode(USTR(x));
}

OSL_SHADEOP void
osl_texture_set_twrap(void* opt, ustring_pod x)
{
    ((TextureOpt*)opt)->twrap = TextureOpt::decode_wrapmode(USTR(x));
}

OSL_SHADEOP void
osl_texture_set_rwrap(void* opt, ustring_pod x)
{
    ((TextureOpt*)opt)->rwrap = TextureOpt::decode_wrapmode(USTR(x));
}

OSL_SHADEOP void
osl_texture_set_stwrap(void* opt, ustring_pod x)
{
    TextureOpt::Wrap code     = TextureOpt::decode_wrapmode(USTR(x));
    ((TextureOpt*)opt)->swrap = code;
    ((TextureOpt*)opt)->twrap = code;
}

OSL_SHADEOP void
osl_texture_set_swrap_code(void* opt, int mode)
{
    ((TextureOpt*)opt)->swrap = (TextureOpt::Wrap)mode;
}

OSL_SHADEOP void
osl_texture_set_twrap_code(void* opt, int mode)
{
    ((TextureOpt*)opt)->twrap = (TextureOpt::Wrap)mode;
}

OSL_SHADEOP void
osl_texture_set_rwrap_code(void* opt, int mode)
{
    ((TextureOpt*)opt)->rwrap = (TextureOpt::Wrap)mode;
}

OSL_SHADEOP void
osl_texture_set_stwrap_code(void* opt, int mode)
{
    ((TextureOpt*)opt)->swrap = (TextureOpt::Wrap)mode;
    ((TextureOpt*)opt)->twrap = (TextureOpt::Wrap)mode;
}

OSL_SHADEOP void
osl_texture_set_sblur(void* opt, float x)
{
    ((TextureOpt*)opt)->sblur = x;
}

OSL_SHADEOP void
osl_texture_set_tblur(void* opt, float x)
{
    ((TextureOpt*)opt)->tblur = x;
}

OSL_SHADEOP void
osl_texture_set_rblur(void* opt, float x)
{
    ((TextureOpt*)opt)->rblur = x;
}

OSL_SHADEOP void
osl_texture_set_stblur(void* opt, float x)
{
    ((TextureOpt*)opt)->sblur = x;
    ((TextureOpt*)opt)->tblur = x;
}

OSL_SHADEOP void
osl_texture_set_swidth(void* opt, float x)
{
    ((TextureOpt*)opt)->swidth = x;
}

OSL_SHADEOP void
osl_texture_set_twidth(void* opt, float x)
{
    ((TextureOpt*)opt)->twidth = x;
}

OSL_SHADEOP void
osl_texture_set_rwidth(void* opt, float x)
{
    ((TextureOpt*)opt)->rwidth = x;
}

OSL_SHADEOP void
osl_texture_set_stwidth(void* opt, float x)
{
    ((TextureOpt*)opt)->swidth = x;
    ((TextureOpt*)opt)->twidth = x;
}

OSL_SHADEOP void
osl_texture_set_fill(void* opt, float x)
{
    ((TextureOpt*)opt)->fill = x;
}

OSL_SHADEOP void
osl_texture_set_time(void* opt, float x)
{
    ((TextureOpt*)opt)->time = x;
}

OSL_SHADEOP int
osl_texture_decode_interpmode(ustring_pod name)
{
    return tex_interp_to_code(ustring_from(USTR(name)));
}

OSL_SHADEOP void
osl_texture_set_interp(void* opt, ustring_pod modename)
{
    int mode = tex_interp_to_code(ustring_from(USTR(modename)));
    if (mode >= 0)
        ((TextureOpt*)opt)->interpmode = (TextureOpt::InterpMode)mode;
}

OSL_SHADEOP void
osl_texture_set_interp_code(void* opt, int mode)
{
    ((TextureOpt*)opt)->interpmode = (TextureOpt::InterpMode)mode;
}

OSL_SHADEOP void
osl_texture_set_subimage(void* opt, int subimage)
{
    ((TextureOpt*)opt)->subimage = subimage;
}


OSL_SHADEOP void
osl_texture_set_subimagename(void* opt, ustring_pod subimagename)
{
    ((TextureOpt*)opt)->subimagename = ustring_from(USTR(subimagename));
}

OSL_SHADEOP void
osl_texture_set_missingcolor_arena(void* opt, const void* missing)
{
    ((TextureOpt*)opt)->missingcolor = (const float*)missing;
}

OSL_SHADEOP void
osl_texture_set_missingcolor_alpha(void* opt, int alphaindex,
                                   float missingalpha)
{
    float* m = (float*)((TextureOpt*)opt)->missingcolor;
    if (m)
        m[alphaindex] = missingalpha;
}



OSL_SHADEOP int
osl_texture(void* sg_, const char* name, void* handle, void* opt_, float s,
            float t, float dsdx, float dtdx, float dsdy, float dtdy, int chans,
            void* result, void* dresultdx, void* dresultdy, void* alpha,
            void* dalphadx, void* dalphady, ustringrep* errormessage)
{
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    TextureOpt* opt   = (TextureOpt*)opt_;
    bool derivs       = (dresultdx || dalphadx);
    // It's actually faster to ask for 4 channels (even if we need fewer)
    // and ensure that they're being put in aligned memory.
    OIIO::simd::vfloat4 result_simd, dresultds_simd, dresultdt_simd;
    ustringhash em;
    bool ok = sg->renderer->texture(
        USTR(name).uhash(), (TextureSystem::TextureHandle*)handle,
        sg->context->texture_thread_info(), *opt, sg, s, t, dsdx, dtdx, dsdy,
        dtdy, 4, (float*)&result_simd, derivs ? (float*)&dresultds_simd : NULL,
        derivs ? (float*)&dresultdt_simd : NULL, errormessage ? &em : nullptr);

    for (int i = 0; i < chans; ++i)
        ((float*)result)[i] = result_simd[i];
    if (alpha)
        ((float*)alpha)[0] = result_simd[chans];

    // Correct our st texture space gradients into xy-space gradients
    if (derivs) {
        OSL_DASSERT((dresultdx == nullptr) == (dresultdy == nullptr));
        OSL_DASSERT((dalphadx == nullptr) == (dalphady == nullptr));
        OIIO::simd::vfloat4 dresultdx_simd = dresultds_simd * dsdx
                                             + dresultdt_simd * dtdx;
        OIIO::simd::vfloat4 dresultdy_simd = dresultds_simd * dsdy
                                             + dresultdt_simd * dtdy;
        if (dresultdx) {
            for (int i = 0; i < chans; ++i)
                ((float*)dresultdx)[i] = dresultdx_simd[i];
            for (int i = 0; i < chans; ++i)
                ((float*)dresultdy)[i] = dresultdy_simd[i];
        }
        if (dalphadx) {
            ((float*)dalphadx)[0] = dresultdx_simd[chans];
            ((float*)dalphady)[0] = dresultdy_simd[chans];
        }
    }

    if (errormessage)
        *errormessage = ok ? ustringrep_from(Strings::_emptystring_)
                           : ustringrep_from(em);
    return ok;
}



OSL_SHADEOP int
osl_texture3d(void* sg_, const char* name, void* handle, void* opt_, void* P_,
              void* dPdx_, void* dPdy_, void* dPdz_, int chans, void* result,
              void* dresultdx, void* dresultdy, void* alpha, void* dalphadx,
              void* dalphady, ustringrep* errormessage)
{
    const Vec3& P(*(Vec3*)P_);
    const Vec3& dPdx(*(Vec3*)dPdx_);
    const Vec3& dPdy(*(Vec3*)dPdy_);
    Vec3 dPdz(0.0f);
    if (dPdz_ != nullptr) {
        dPdz = (*(Vec3*)dPdz_);
    }
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    TextureOpt* opt   = (TextureOpt*)opt_;
    bool derivs       = (dresultdx != NULL || dalphadx != NULL);
    // It's actually faster to ask for 4 channels (even if we need fewer)
    // and ensure that they're being put in aligned memory.
    OIIO::simd::vfloat4 result_simd, dresultds_simd, dresultdt_simd,
        dresultdr_simd;
    ustringhash em;
    bool ok = sg->renderer->texture3d(
        USTR(name).uhash(), (TextureSystem::TextureHandle*)handle,
        sg->context->texture_thread_info(), *opt, sg, P, dPdx, dPdy, dPdz, 4,
        (float*)&result_simd, derivs ? (float*)&dresultds_simd : nullptr,
        derivs ? (float*)&dresultdt_simd : nullptr,
        derivs ? (float*)&dresultdr_simd : nullptr,
        errormessage ? &em : nullptr);

    for (int i = 0; i < chans; ++i)
        ((float*)result)[i] = result_simd[i];
    if (alpha)
        ((float*)alpha)[0] = result_simd[chans];

    // Correct our str texture space gradients into xyz-space gradients
    if (derivs) {
        OIIO::simd::vfloat4 dresultdx_simd = dresultds_simd * dPdx.x
                                             + dresultdt_simd * dPdx.y
                                             + dresultdr_simd * dPdx.z;
        OIIO::simd::vfloat4 dresultdy_simd = dresultds_simd * dPdy.x
                                             + dresultdt_simd * dPdy.y
                                             + dresultdr_simd * dPdy.z;
        if (dresultdx) {
            for (int i = 0; i < chans; ++i)
                ((float*)dresultdx)[i] = dresultdx_simd[i];
            for (int i = 0; i < chans; ++i)
                ((float*)dresultdy)[i] = dresultdy_simd[i];
        }
        if (dalphadx) {
            ((float*)dalphadx)[0] = dresultdx_simd[chans];
            ((float*)dalphady)[0] = dresultdy_simd[chans];
        }
    }

    if (errormessage)
        *errormessage = ok ? ustringrep_from(Strings::_emptystring_)
                           : ustringrep_from(em);
    return ok;
}



OSL_SHADEOP int
osl_environment(void* sg_, const char* name, void* handle, void* opt_, void* R_,
                void* dRdx_, void* dRdy_, int chans, void* result,
                void* dresultdx, void* dresultdy, void* alpha, void* dalphadx,
                void* dalphady, ustringrep* errormessage)
{
    const Vec3& R(*(Vec3*)R_);
    const Vec3& dRdx(*(Vec3*)dRdx_);
    const Vec3& dRdy(*(Vec3*)dRdy_);
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    TextureOpt* opt   = (TextureOpt*)opt_;
    // It's actually faster to ask for 4 channels (even if we need fewer)
    // and ensure that they're being put in aligned memory.
    OIIO::simd::vfloat4 local_result;
    ustringhash em;
    bool ok = sg->renderer->environment(USTR(name).uhash(),
                                        (TextureSystem::TextureHandle*)handle,
                                        sg->context->texture_thread_info(),
                                        *opt, sg, R, dRdx, dRdy, 4,
                                        (float*)&local_result, NULL, NULL,
                                        errormessage ? &em : nullptr);

    for (int i = 0; i < chans; ++i)
        ((float*)result)[i] = local_result[i];

    // For now, just zero out the result derivatives.  If somebody needs
    // derivatives of environment lookups, we'll fix it.  The reason
    // that this is a pain is that OIIO's environment call (unwisely?)
    // returns the st gradients, but we want the xy gradients, which is
    // tricky because we (this function you're reading) don't know which
    // projection is used to generate st from R.  Ugh.  Sweep under the
    // rug for a day when somebody is really asking for it.
    if (dresultdx) {
        for (int i = 0; i < chans; ++i)
            ((float*)dresultdx)[i] = 0.0f;
        for (int i = 0; i < chans; ++i)
            ((float*)dresultdy)[i] = 0.0f;
    }
    if (alpha) {
        ((float*)alpha)[0] = local_result[chans];
        // Zero out the alpha derivatives, for the same reason as above.
        if (dalphadx)
            ((float*)dalphadx)[0] = 0.0f;
        if (dalphady)
            ((float*)dalphady)[0] = 0.0f;
    }

    if (errormessage)
        *errormessage = ok ? ustringrep_from(Strings::_emptystring_)
                           : ustringrep_from(em);
    return ok;
}



OSL_SHADEOP int
osl_get_textureinfo(void* sg_, const char* name, void* handle, void* dataname,
                    int type, int arraylen, int aggregate, void* data,
                    ustringrep* errormessage)
{
    // recreate TypeDesc
    TypeDesc typedesc;
    typedesc.basetype  = type;
    typedesc.arraylen  = arraylen;
    typedesc.aggregate = aggregate;

    ShaderGlobals* sg = (ShaderGlobals*)sg_;

    ustringhash em;
    bool ok = sg->renderer->get_texture_info(
        USTR(name).uhash(), (RendererServices::TextureHandle*)handle,
        sg->context->texture_thread_info(), sg->context, 0 /*FIXME-ptex*/,
        USTR(dataname).uhash(), typedesc, data, errormessage ? &em : nullptr);
    if (errormessage)
        *errormessage = ok ? ustringrep_from(Strings::_emptystring_)
                           : ustringrep_from(em);
    return ok;
}



OSL_SHADEOP int
osl_get_textureinfo_st(void* sg_, const char* name, void* handle, float s,
                       float t, void* dataname, int type, int arraylen,
                       int aggregate, void* data, ustringrep* errormessage)
{
    // recreate TypeDesc
    TypeDesc typedesc;
    typedesc.basetype  = type;
    typedesc.arraylen  = arraylen;
    typedesc.aggregate = aggregate;

    ShaderGlobals* sg = (ShaderGlobals*)sg_;

    ustringhash em;
    bool ok = sg->renderer->get_texture_info(
        USTR(name).uhash(), (RendererServices::TextureHandle*)handle, s, t,
        sg->context->texture_thread_info(), sg->context, 0 /*FIXME-ptex*/,
        USTR(dataname).uhash(), typedesc, data, errormessage ? &em : nullptr);
    if (errormessage)
        *errormessage = ok ? ustringrep_from(Strings::_emptystring_)
                           : ustringrep_from(em);
    return ok;
}



// Trace

// Utility: retrieve a pointer to the ShadingContext's trace options
// struct, also re-initialize its contents.
OSL_SHADEOP void*
osl_get_trace_options(void* sg_)
{
    ShaderGlobals* sg               = (ShaderGlobals*)sg_;
    RendererServices::TraceOpt* opt = sg->context->trace_options_ptr();
    new (opt) RendererServices::TraceOpt;
    return opt;
}

OSL_SHADEOP void
osl_trace_set_mindist(void* opt, float x)
{
    ((RendererServices::TraceOpt*)opt)->mindist = x;
}

OSL_SHADEOP void
osl_trace_set_maxdist(void* opt, float x)
{
    ((RendererServices::TraceOpt*)opt)->maxdist = x;
}

OSL_SHADEOP void
osl_trace_set_shade(void* opt, int x)
{
    ((RendererServices::TraceOpt*)opt)->shade = x;
}


OSL_SHADEOP void
osl_trace_set_traceset(void* opt, const char* x)
{
    ((RendererServices::TraceOpt*)opt)->traceset = USTR(x);
}


OSL_SHADEOP int
osl_trace(void* sg_, void* opt_, void* Pos_, void* dPosdx_, void* dPosdy_,
          void* Dir_, void* dDirdx_, void* dDirdy_)
{
    ShaderGlobals* sg               = (ShaderGlobals*)sg_;
    RendererServices::TraceOpt* opt = (RendererServices::TraceOpt*)opt_;
    static const Vec3 Zero(0.0f, 0.0f, 0.0f);
    const Vec3* Pos    = (Vec3*)Pos_;
    const Vec3* dPosdx = dPosdx_ ? (Vec3*)dPosdx_ : &Zero;
    const Vec3* dPosdy = dPosdy_ ? (Vec3*)dPosdy_ : &Zero;
    const Vec3* Dir    = (Vec3*)Dir_;
    const Vec3* dDirdx = dDirdx_ ? (Vec3*)dDirdx_ : &Zero;
    const Vec3* dDirdy = dDirdy_ ? (Vec3*)dDirdy_ : &Zero;
    return sg->renderer->trace(*opt, sg, *Pos, *dPosdx, *dPosdy, *Dir, *dDirdx,
                               *dDirdy);
}


}  // namespace pvt
OSL_NAMESPACE_EXIT
