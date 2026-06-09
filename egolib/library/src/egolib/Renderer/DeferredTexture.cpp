#include "egolib/Renderer/DeferredTexture.hpp"
#include "egolib/Graphics/TextureManager.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/fileutil.h"
#include "egolib/Image/ImageManager.hpp"

namespace Ego {

DeferredTexture::DeferredTexture() :
    _texture(nullptr),
    _textureHD(nullptr),
    _loaded(false),
    _loadedHD(false),
    _hasFilteringOverride(false),
    _minFilterOverride(idlib::texture_filter_method::none),
    _magFilterOverride(idlib::texture_filter_method::none),
    _mipMapFilterOverride(idlib::texture_filter_method::none),
    _filePath() {
    //default ctor invalid texture
}


DeferredTexture::DeferredTexture(const std::string &filePath) :
    _texture(nullptr),
    _textureHD(nullptr),
    _loaded(false),
    _loadedHD(false),
    _hasFilteringOverride(false),
    _minFilterOverride(idlib::texture_filter_method::none),
    _magFilterOverride(idlib::texture_filter_method::none),
    _mipMapFilterOverride(idlib::texture_filter_method::none),
    _filePath(filePath) {
    //Do not load texture until its needed
}

std::shared_ptr<const Texture> DeferredTexture::get() const {
    if (!_loaded) {
        if (_filePath.empty()) {
            throw std::logic_error("DeferredTexture::get() on nullptr texture");
        }

        _texture = TextureManager::get().getTexture(_filePath);
        applyFilteringOverride(_texture);
        _loaded = true;
    }

    //Load and use the optional HD texture if it is available (else fall back to normal texture)
    if(Ego::activeConfig().graphic_hd_textures_enable.getValue()) {

        if(!_loadedHD) {
            if(ego_texture_exists_vfs(_filePath + "_HD")) {
                _textureHD = TextureManager::get().getTexture(_filePath + "_HD");
                applyFilteringOverride(_textureHD);
            } 

            _loadedHD = true;            
        }

        if(_textureHD != nullptr) {
            return _textureHD; //Oh yeah HD!
        }
    }

    return _texture;
}

void DeferredTexture::release() {
    _loaded = false;
    _loadedHD = false;
    _texture.reset();
    _textureHD.reset();
}

void DeferredTexture::setTextureSource(const std::string &filePath) {
    // Release any old source first
    release();

    _filePath = filePath;
}

void DeferredTexture::setFiltering(idlib::texture_filter_method minFilter,
                                   idlib::texture_filter_method magFilter,
                                   idlib::texture_filter_method mipMapFilter) {
    _hasFilteringOverride = true;
    _minFilterOverride = minFilter;
    _magFilterOverride = magFilter;
    _mipMapFilterOverride = mipMapFilter;
    applyFilteringOverride(_texture);
    applyFilteringOverride(_textureHD);
}

void DeferredTexture::applyFilteringOverride(const std::shared_ptr<Texture>& texture) const {
    if (!_hasFilteringOverride || !texture) {
        return;
    }
    texture->setMinFilter(_minFilterOverride);
    texture->setMagFilter(_magFilterOverride);
    texture->setMipMapFilter(_mipMapFilterOverride);
}

const std::string& DeferredTexture::getFilePath() const {
    return _filePath;
}

} // namespace Ego
