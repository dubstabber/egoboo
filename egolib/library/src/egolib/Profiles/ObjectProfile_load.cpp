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
#include "idlib/exception.hpp"  // idlib::exception (base of the parser exceptions)

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
        // Load the model for this profile.
        //
        // The std::runtime_error arm below is correct for the primary failures: ModelDescriptor's
        // constructor throws std::runtime_error at each of its four rejection points (unsupported
        // format, no model file found, model failed to load, glTF metadata missing). But the MD2
        // branch of that constructor also calls initializeFromLegacyFrames, whose collectHealAliases
        // (ModelAnimationMetadata_legacy.cpp) guards only the ReadContext construction - the
        // skipToColon/vfs_read_string_lit loop that follows can raise idlib::hll::compilation_error
        // on a copy.txt that opens but is malformed. idlib::exception has no std::exception base
        // (idlib/exception/exception.hpp:64), so that escaped this handler entirely. Same disposition
        // as the data.txt handler further down: one bad object is skipped, not fatal.
        try
        {
            profile->_model = std::make_shared<Ego::ModelDescriptor>(folderPath.c_str());
        }
        catch (const idlib::exception &ex)
        {
            // The message is the whole point of this arm: a compilation_error out of
            // collectHealAliases names copy.txt and the line and column that failed, none of
            // which the folder path alone conveys. Flattened because
            // idlib::runtime_error::to_string() is multi-line by design (runtime_error.hpp emits
            // "runtime error:", the raise site and the message on separate lines).
            std::string reason = ex.to_string();
            for (char& c : reason)
            {
                if (c == '\n' || c == '\r') c = ' ';
            }
            services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load model ", "`", folderPath, "`", ": ", reason, Log::EndOfEntry);
            return nullptr;
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
    // One unparsable object must not abort the caller's whole scan, so this reports the object
    // as unloadable (nullptr) rather than letting the throw out. Every caller already treats
    // nullptr as "skip this object": ProfileSystem::loadOneProfile logs and returns
    // ObjectProfileRef::Invalid, ProfileSystem::loadAllSavedCharacters continues its search,
    // and the content validator records a parse_failure for the object.
    //
    // The idlib arm is the one that matters here. loadDataFile opens `ReadContext ctxt(filePath)`
    // as its first statement (ObjectProfile_data.cpp) and then runs ~200 vfs_get_next_* calls
    // over it, and everything on that path raises idlib types: idlib::runtime_error from
    // vfs_readEntireFile (vfs_bulk.c:56) by way of the Ego::Script::Scanner constructor when
    // data.txt is missing or unreadable, and idlib::hll::compilation_error - plus its subclass
    // Ego::Script::MissingDelimiterError (Script/Errors.hpp:29) - from the ReadContext parse
    // helpers on a malformed or truncated one. idlib::exception (idlib/exception/exception.hpp:64)
    // is declared with NO base class, so the std::runtime_error arm that used to stand alone here
    // caught nothing loadDataFile actually throws. That arm is left exactly as it was, and
    // deliberately NOT widened to std::exception: ObjectProfile_data.cpp contains no throw of its
    // own, so widening would add reach for std::bad_alloc and nothing else, and an allocation
    // failure reported as "failed to parse data.txt" is a fabricated content fault. It keeps
    // propagating instead, out to LoadingState::loadModuleData or the validator's top level. The
    // same disposition as the model-load handler above and as ModuleProfile::moduleHasIDSZ.
    // Catch by reference only - idlib::exception's copy constructor and destructor are protected,
    // so a by-value handler would not compile.
    try
    {
        if (!profile->loadDataFile(folderPath + "/data.txt", services))
        {
            services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load data.txt for profile ", "`", folderPath, "`", Log::EndOfEntry);
            return nullptr;
        }
    }
    catch (const idlib::exception &ex)
    {
        // idlib::runtime_error::to_string() is multi-line by design (runtime_error.hpp emits
        // "runtime error:", the raise site and the message on separate lines). Flatten it so one
        // rejected object stays one log record.
        std::string reason = ex.to_string();
        for (char& c : reason)
        {
            if (c == '\n' || c == '\r') c = ' ';
        }
        services.logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to parse ", "`", folderPath, "/data.txt", "`", ": ", reason, Log::EndOfEntry);
        return nullptr;
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
