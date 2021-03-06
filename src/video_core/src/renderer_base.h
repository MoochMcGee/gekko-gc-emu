/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    renderer_base.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-03-08
 * @brief   Renderer base class for new video core
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#ifndef VIDEO_CORE_RENDER_BASE_H_
#define VIDEO_CORE_RENDER_BASE_H_

#include "common.h"
#include "hash.h"
#include "fifo.h"
#include "gx_types.h"
#include "video/emuwindow.h"
#include "vertex_loader.h"
#include "shader_manager.h"
#include "texture_manager.h"

class RendererBase {
public:

    /// Used to reference a framebuffer
    enum kFramebuffer {
        kFramebuffer_VirtualXFB = 0,
        kFramebuffer_EFB,
        kFramebuffer_Texture
    };

    /// Used for referencing the render modes
    enum kRenderMode {
        kRenderMode_None = 0,
        kRenderMode_Multipass = 1,
        kRenderMode_ZComp = 2,
        kRenderMode_UseDstAlpha = 4
    };

    RendererBase() : current_fps_(0), current_frame_(0), texture_interface_(NULL) {
    }

    ~RendererBase() {
    }

    /**
     * Write data to BP for renderer internal use (e.g. direct to shader)
     * @param addr BP register address
     * @param data Value to write to BP register
     */
    virtual void WriteBP(u8 addr, u32 data) = 0;

    /**
     * Write data to CP for renderer internal use (e.g. direct to shader)
     * @param addr CP register address
     * @param data Value to write to CP register
     */
    virtual void WriteCP(u8 addr, u32 data) = 0;

    /**
     * Write data to XF for renderer internal use (e.g. direct to shader)
     * @param addr XF address
     * @param length Length (in 32-bit words) to write to XF
     * @param data Data buffer to write to XF
     */
    virtual void WriteXF(u16 addr, int length, u32* data) = 0;

    /**
     * Begin renderering of a primitive
     * @param prim Primitive type (e.g. GX_TRIANGLES)
     * @param count Number of vertices to be drawn (used for appropriate memory management, only)
     * @param vbo Pointer to VBO, which will be set by API in this function
     * @param vbo_offset Offset into VBO to use (in bytes)
     */
    virtual void BeginPrimitive(GXPrimitive prim, int count, GXVertex** vbo, u32 vbo_offset) = 0;

    /**
     * Set the current vertex state (format and count of each vertex component)
     * @param vertex_state VertexState structure of the current vertex state
     */
    virtual void SetVertexState(const gp::VertexState& vertex_state) = 0;

    /**
     * Used to signal to the render that a region in XF is required by a primitive
     * @param index Vector index in XF memory that is required
     */
    virtual void VertexPosition_UseIndexXF(u8 index) = 0;

    /// End a primitive (signal renderer to draw it)
    virtual void EndPrimitive(u32 vbo_offset, u32 vertex_num) = 0;
   
    /// Sets the render viewport location, width, and height
    virtual void SetViewport(int x, int y, int width, int height) = 0;

    /// Swap buffers (render frame)
    virtual void SwapBuffers() = 0;

    /// Sets the renderer depthrange, znear and zfar
    virtual void SetDepthRange(double znear, double zfar) = 0;

    /// Sets the renderer depth test mode
    virtual void SetDepthMode() = 0;

    /// Sets the renderer generation mode
    virtual void SetGenerationMode() = 0;

    /** 
     * Sets the renderer blend mode
     * @param pe_cmode_0 BPPECMode0 register to user for blend settings
     * @param pe_cmode_1 BPPECMode1 register to user for blend settings
     * @param blend_mode_ Forces blend mode to update
     */
    virtual void SetBlendMode(const gp::BPPECMode0& pe_cmode_0, const gp::BPPECMode1& pe_cmode_1, 
        bool force_update) = 0;

    /** 
     * Sets the renderer logic op mode
     * @param pe_cmode_0 BPPECMode0 register to user for blend settings
     */
    virtual void SetLogicOpMode(const gp::BPPECMode0& pe_cmode_0) = 0;

    /**
     * Sets the renderer dither mode
     * @param pe_cmode_0 BPPECMode0 register to user for blend settings
     */
    virtual void SetDitherMode(const gp::BPPECMode0& pe_cmode_0) = 0;

    /**
     * Sets the renderer color mask mode
     * @param pe_cmode_0 BPPECMode0 register to user for blend settings
     */
    virtual void SetColorMask(const gp::BPPECMode0& pe_cmode_0) = 0;

    /* Sets the scissor box
     * @param rect Renderer rectangle to set scissor box to
     */
    virtual void SetScissorBox(const Rect& rect) = 0;

    /**
     * Sets the line and point size
     * @param line_width Line width to use
     * @param point_size Point size to use
     */
    virtual void SetLinePointSize(f32 line_width, f32 point_size) = 0;

    /** 
     * Blits the EFB to the external framebuffer (XFB)
     * @param src_rect Source rectangle in EFB to copy
     * @param dst_rect Destination rectangle in EFB to copy to
     * @param dest_height Destination height in pixels
     */
    virtual void CopyToXFB(const Rect& src_rect, const Rect& dst_rect) = 0;

    /**
     * Clear the screen
     * @param rect Screen rectangle to clear
     * @param enable_color Enable color clearing
     * @param enable_alpha Enable alpha clearing
     * @param enable_z Enable depth clearing
     * @param color Clear color
     * @param z Clear depth
     */
    virtual void Clear(const Rect& rect, bool enable_color, bool enable_alpha, bool enable_z, 
        u32 color, u32 z) = 0;

    /**
     * Set a specific render mode
     * @param flag Render flags mode to enable
     */
    virtual void SetMode(kRenderMode flags) = 0;

    /**
     * Restore the render mode
     * @param pe_cmode_0 BPPECMode0 register to user for blend settings
     */
    virtual void RestoreMode(const gp::BPPECMode0& pe_cmode_0) = 0;

    /// Reset the full renderer API to the NULL state
    virtual void ResetRenderState() = 0;

    /// Restore the full renderer API state - As the game set it
    virtual void RestoreRenderState() = 0;

    /** 
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    virtual void SetWindow(EmuWindow* window) = 0;

    /// Initialize the renderer
    virtual void Init() = 0;

    /// Shutdown the renderer
    virtual void ShutDown() = 0;

    /// Converts EFB rectangle coordinates to renderer rectangle coordinates
    static Rect EFBToRendererRect(const Rect& rect) {
	    return Rect(rect.x0_, kGCEFBHeight - rect.y0_, rect.x1_, kGCEFBHeight - rect.y1_);
    }

    // Getter/setter functions:
    // ------------------------

    f32 current_fps() const { return current_fps_; }

    int current_frame() const { return current_frame_; }

    ShaderManager::BackendInterface* shader_interface() const { return shader_interface_; }

    TextureManager::BackendInterface* texture_interface() const { return texture_interface_; }

protected:
    f32 current_fps_;                       ///< Current framerate, should be set by the renderer
    int current_frame_;                     ///< Current frame, should be set by the renderer

    ShaderManager::BackendInterface*    shader_interface_;
    TextureManager::BackendInterface*   texture_interface_;

private:

    DISALLOW_COPY_AND_ASSIGN(RendererBase);
};

#endif // VIDEO_CORE_RENDER_BASE_H_
