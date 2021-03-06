/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    texture_interface.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-12-05
 * @brief   Texture manager interface for the GL3 renderer
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

#ifndef VIDEO_CORE_RENDERER_GL3_TEXTURE_INTERFACE_H_
#define VIDEO_CORE_RENDERER_GL3_TEXTURE_INTERFACE_H_

#include <GL/glew.h>

#include "types.h"
#include "bp_mem.h"
#include "texture_manager.h"
#include "renderer_gl3.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// GL3 texture interface implementation

/// Storage container for cached textures in the GL3 renderer
class TextureInterface : virtual public TextureManager::BackendInterface {
public:

    TextureInterface(RendererGL3* parent);
    ~TextureInterface();

    /**
     * Create a new texture in the backend renderer
     * @param active_texture_unit Active texture unit to bind to for creation
     * @param cache_entry CacheEntry to create texture for
     * @param raw_data Raw texture data
     * @return a pointer to CacheEntry::BackendData with renderer-specific texture data
     */
    TextureManager::CacheEntry::BackendData* Create(int active_texture_unit, 
        const TextureManager::CacheEntry& cache_entry, u8* raw_data);

    /**
     * Delete a texture from the backend renderer
     * @param backend_data Renderer-specific texture data used by renderer to remove it
     */
    void Delete(TextureManager::CacheEntry::BackendData* backend_data);

    /** 
     * Call to update a texture with a new EFB copy of the region specified by rect
     * @param src_rect Source rectangle to copy from EFB
     * @param dst_rect Destination rectange to copy to
     * @param backend_data Pointer to renderer-specific data used for the EFB copy
     */
    void CopyEFB(const Rect& src_rect, const Rect& dst_rect,
        const TextureManager::CacheEntry::BackendData* backend_data);

    /**
     * Binds a texture to the backend renderer
     * @param active_texture_unit Active texture unit to bind to
     * @param backend_data Pointer to renderer-specific data used for binding
     */
    void Bind(int active_texture_unit, const TextureManager::CacheEntry::BackendData* backend_data);

    /**
     * Updates the texture parameters
     * @param active_texture_unit Active texture unit to update the parameters for
     * @param tex_mode_0 BP TexMode0 register to use for the update
     * @param tex_mode_1 BP TexMode1 register to use for the update
     */
    void UpdateParameters(int active_texture_unit, const gp::BPTexMode0& tex_mode_0,
        const gp::BPTexMode1& tex_mode_1);

private:

    RendererGL3* parent_;

    class BackendData : public TextureManager::CacheEntry::BackendData {
    public:
        BackendData() : color_texture_(0), depth_texture_(0), efb_framebuffer_(0), 
            is_depth_copy(false) {
        }
        ~BackendData() {
        }
        GLuint color_texture_;      ///< GL handle to color texture
        GLuint depth_texture_;      ///< GL handle to depth texture (if from an EFB copy)
        GLuint efb_framebuffer_;    ///< GL handle to EFB copy FBO

        bool is_depth_copy;
    };

    DISALLOW_COPY_AND_ASSIGN(TextureInterface);
};

#endif // VIDEO_CORE_RENDERER_GL3_TEXTURE_INTERFACE_H_
