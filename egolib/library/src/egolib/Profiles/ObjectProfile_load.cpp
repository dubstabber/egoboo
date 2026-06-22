//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/Profiles/ObjectProfile_load.cpp
/// @brief ObjectProfile loading and parsing helpers.

#include "egolib/Profiles/ObjectProfile_internal.h"
#include "egolib/fileutil.h"
#include "egolib/Image/ImageManager.hpp"

void ObjectProfile::loadTextures(const std::string &folderPath, const LoadServices& services)
{
    //Clear texture references
    _texturesLoaded.clear();
    _iconsLoaded.clear();

    // Load the skins and icons
    for (size_t cnt = 0; cnt < 30; cnt++)
    {
        // do the texture
        const std::string skinPath = folderPath + "/tris" + std::to_string(cnt);
        if(ego_texture_exists_vfs(skinPath))
        {
            _texturesLoaded[cnt] = Ego::DeferredTexture(skinPath);

            // palshad's Golden Key uses a tiny legacy skin that collapses into a
            // flat-looking blob under the modern global mip/filter settings when
            // viewed at slight camera angles. Keep its original crisp sampling local
            // to this object instead of globally lowering texture quality.
            if (0 == cnt && std::string::npos != folderPath.find("palshad.mod/objects/keya.obj"))
            {
                _texturesLoaded[cnt].setFiltering(idlib::texture_filter_method::nearest,
                                                  idlib::texture_filter_method::nearest,
                                                  idlib::texture_filter_method::none);
            }
        }

        // do the icon
        const std::string iconPath = folderPath + "/icon" + std::to_string(cnt);
        if(ego_texture_exists_vfs(iconPath))
        {
            _iconsLoaded[cnt] = Ego::DeferredTexture(iconPath);
        }
    }

    // If we didn't get a skin, set it to the water texture
    if (_texturesLoaded.empty())
    {
        _texturesLoaded[0] = Ego::DeferredTexture("mp_data/waterlow");
        services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "object is missing a skin ", "`", getPathname(), "`", Log::EndOfEntry);
    }

    // If we didn't get a icon, set it to the NULL icon
    if (_iconsLoaded.empty())
    {
        _iconsLoaded[0] = Ego::DeferredTexture("mp_data/nullicon");
        services.logTarget << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "object is missing an icon ", "`", getPathname(), "`", Log::EndOfEntry);
    }
}

void ObjectProfile::loadAllMessages(const std::string &filePath)
{
    /// @author ZF
    /// @details This function loads all messages for an object

    std::unique_ptr<ReadContext> ctxt = nullptr;
    try {
        ctxt = std::make_unique<ReadContext>(filePath);
    } catch (...) {
        return;
    }

    while (ctxt->skipToColon(true))
    {
        //Load one line
        addMessage(vfs_read_string_lit(*ctxt));
    }
}

std::shared_ptr<ObjectProfile> ObjectProfile::loadFromFile(const std::string& folderPath, ObjectProfileRef ref, bool lightWeight)
{
    const LoadServices services{
        Log::activeTarget(),
        Ego::Perks::activePerkHandler(),
        activeProfileSystem(),
        Ego::activeConfig(),
        !lightWeight ? tryActiveAudioSystem() : nullptr
    };

    // Assert the reference is valid.
    if (!ref)
    {
        services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "invalid profile reference ", ref, Log::EndOfEntry);
        return nullptr;
    }

    // Allocate the object profile object.
    std::shared_ptr<ObjectProfile> profile = std::make_shared<ObjectProfile>();

    // Set some data
    profile->_pathname = folderPath;
    profile->_slotNumber = ref.get();

    //Don't load 3d model, enchant, messages, sounds or particle effects for lightweight profiles
    if (!lightWeight)
    {
        // Load the model for this profile
        try
        {
            profile->_model = std::make_shared<Ego::ModelDescriptor>(folderPath.c_str());
        }
        catch (const std::runtime_error &ex)
        {
            services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load model ", "`", folderPath, "`", Log::EndOfEntry);
            return nullptr;
        }

        // Load the enchantment for this profile (optional)
        profile->_ieve = services.profileSystem.loadEnchantProfile(folderPath + "/enchant.txt",
                                                                   static_cast<EVE_REF>(ref.get()));

        // Load the messages for this profile, do this before loading the AI script
        // to ensure any dynamic loaded messages get loaded last (optional)
        profile->loadAllMessages(folderPath + "/message.txt");

        // Load the particles for this profile (optional)
        for (LocalParticleProfileRef cnt(0); cnt.get() < 30; ++cnt) //TODO: find better way of listing files
        {
            const std::string particleName = folderPath + "/part" + std::to_string(cnt.get()) + ".txt";
            PIP_REF particleProfile = services.profileSystem.loadParticleProfile(particleName.c_str(),
                                                                                INVALID_PIP_REF);

            // Make sure it's referenced properly
            if (particleProfile != INVALID_PIP_REF)
            {
                profile->_particleProfiles[cnt] = particleProfile;
            }
        }

        // Load the waves for this iobj
        for (size_t cnt = 0; cnt < 30; cnt++) //TODO: make better search than just 30 (list files?)
        {
            const std::string soundName = folderPath + "/sound" + std::to_string(cnt);
            SoundID soundID = services.audioSystem ? services.audioSystem->loadSound(soundName) : INVALID_SOUND_ID;

            if (soundID != INVALID_SOUND_ID)
            {
                profile->_soundMap[cnt] = soundID;
            }
        }
    }

    //Load profile graphics (optional)
    profile->loadTextures(folderPath, services);

    // Load the random naming table for this icap (optional)
    profile->_randomName.loadFromFile(folderPath + "/naming.txt");

    // Finally load the character profile
    // Do after loading particle and sound profiles
    try
    {
        if (!profile->loadDataFile(folderPath + "/data.txt", services))
        {
            services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load data.txt for profile ", "`", folderPath, "`", Log::EndOfEntry);
            return nullptr;
        }
    }
    catch (const std::runtime_error &ex)
    {
        services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to parse ", "`", folderPath, "/data.txt", "`", ": ", ex.what(), Log::EndOfEntry);
        return nullptr;
    }

    // Fix lighting if need be
    if (profile->_uniformLit && services.config.graphic_gouraudShading_enable.getValue())
    {
        profile->getModel()->makeEquallyLit();
    }

    return profile;
}

std::shared_ptr<ObjectProfile> ObjectProfile::loadFromFile(const std::string &folderPath, PRO_REF ref, const bool lightWeight)
{
    return loadFromFile(folderPath, ObjectProfileRef(ref), lightWeight);
}
