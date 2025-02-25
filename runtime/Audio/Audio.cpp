/*
Copyright(c) 2016-2022 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES =============================
#include "pch.h"
#include "Audio.h"
#include "../Profiling/Profiler.h"
#include "../World/Components/Transform.h"
SP_WARNINGS_OFF
#include <fmod.hpp>
#include <fmod_errors.h>
SP_WARNINGS_ON
//========================================

//= NAMESPACES ======
using namespace std;
using namespace FMOD;
//===================

namespace Spartan
{
    Audio::Audio(Context* context) : ISystem(context)
    {

    }

    Audio::~Audio()
    {
        if (!m_system_fmod)
            return;

        // Close FMOD
        m_result_fmod = m_system_fmod->close();
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            return;
        }

        // Release FMOD
        m_result_fmod = m_system_fmod->release();
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
        }
    }

    void Audio::OnInitialise()
    {
        // Create FMOD instance
        m_result_fmod = System_Create(&m_system_fmod);
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            SP_ASSERT(0 && "Failed to create FMOD instance");
        }

        // Get FMOD version
        uint32_t version;
        m_result_fmod = m_system_fmod->getVersion(&version);
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            SP_ASSERT(0 && "Failed to get FMOD version");
        }

        // Make sure there is a sound device on the machine
        int driver_count = 0;
        m_result_fmod = m_system_fmod->getNumDrivers(&driver_count);
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            SP_ASSERT(0 && "Failed to get a sound device");
        }

        // Initialise FMOD
        m_result_fmod = m_system_fmod->init(m_max_channels, FMOD_INIT_NORMAL, nullptr);
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            SP_ASSERT(0 && "Failed to initialise FMOD");
        }

        // Set 3D settings
        m_result_fmod = m_system_fmod->set3DSettings(1.0, m_distance_entity, 0.0f);
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            SP_ASSERT(0 && "Failed to set 3D settomgs");
        }

        // Get version
        stringstream ss;
        ss << hex << version;
        const auto major = ss.str().erase(1, 4);
        const auto minor = ss.str().erase(0, 1).erase(2, 2);
        const auto rev = ss.str().erase(0, 3);
        Settings::RegisterThirdPartyLib("FMOD", major + "." + minor + "." + rev, "https://www.fmod.com/");

        // Get dependencies
        m_profiler = m_context->GetSystem<Profiler>();

        // Subscribe to events
        SP_SUBSCRIBE_TO_EVENT(EventType::WorldClear, SP_EVENT_HANDLER_EXPRESSION
        (
            m_listener = nullptr;
        ));
    }

    void Audio::OnTick(double delta_time)
    {
        // Don't play audio if the engine is not in game mode
        if (!m_context->m_engine->IsFlagSet(EngineMode::Game))
            return;

        SP_SCOPED_TIME_BLOCK(m_profiler);

        // Update FMOD
        m_result_fmod = m_system_fmod->update();
        if (m_result_fmod != FMOD_OK)
        {
            LogErrorFmod(m_result_fmod);
            return;
        }

        if (m_listener)
        {
            auto position = m_listener->GetPosition();
            auto velocity = Math::Vector3::Zero;
            auto forward = m_listener->GetForward();
            auto up = m_listener->GetUp();

            // Set 3D attributes
            m_result_fmod = m_system_fmod->set3DListenerAttributes(
                0, 
                reinterpret_cast<FMOD_VECTOR*>(&position), 
                reinterpret_cast<FMOD_VECTOR*>(&velocity), 
                reinterpret_cast<FMOD_VECTOR*>(&forward), 
                reinterpret_cast<FMOD_VECTOR*>(&up)
            );
            if (m_result_fmod != FMOD_OK)
            {
                LogErrorFmod(m_result_fmod);
                return;
            }
        }
    }

    void Audio::SetListenerTransform(Transform* transform)
    {
        m_listener = transform;
    }

    void Audio::LogErrorFmod(int error) const
    {
        SP_LOG_ERROR("%s", FMOD_ErrorString(static_cast<FMOD_RESULT>(error)));
    }
}
