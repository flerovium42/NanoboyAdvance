///////////////////////////////////////////////////////////////////////////////////
//
//  NanoboyAdvance is a modern Game Boy Advance emulator written in C++
//  with performance, platform independency and reasonable accuracy in mind.
//  Copyright (C) 2016 Frederic Meyer
//
//  This file is part of nanoboyadvance.
//
//  nanoboyadvance is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  nanoboyadvance is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with nanoboyadvance. If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////


#include "gba.h"
#include "audio/sdl_adapter.h"
#include "config/config.h"
#include <stdexcept>
#include <iostream>


using namespace std;
using namespace Common;


namespace GBA
{
    // Defines the amount of cycles per frame
    const int GBA::FRAME_CYCLES = 280896;


    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      Constructor, 1
    ///
    ///////////////////////////////////////////////////////////
    GBA::GBA(string rom_file, string save_file)
    {
        m_Memory.Init(rom_file, save_file);
        m_ARM.Init(&m_Memory, true);

        // Rudimentary Audio setup
        SDL2AudioAdapter adapter;
        adapter.Init(&m_Memory.m_Audio);
    }

    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      Constructor, 2
    ///
    ///////////////////////////////////////////////////////////
    GBA::GBA(string rom_file, string save_file, string bios_file)
    {
        u8* bios;
        size_t bios_size;

        if (!File::Exists(bios_file))
            throw runtime_error("Cannot open BIOS file.");

        bios = File::ReadFile(bios_file);
        bios_size = File::GetFileSize(bios_file);

        m_Memory.Init(rom_file, save_file, bios, bios_size);
        m_ARM.Init(&m_Memory, false);

        // Rudimentary Audio setup
        SDL2AudioAdapter adapter;
        adapter.Init(&m_Memory.m_Audio);
    }

    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      Frame
    ///
    ///////////////////////////////////////////////////////////
    void GBA::Frame()
    {
        int total_cycles = FRAME_CYCLES * m_SpeedMultiplier;

        m_DidRender = false;

        for (int i = 0; i < total_cycles; ++i)
        {
            u32 interrupts = m_Memory.m_Interrupt.ie & m_Memory.m_Interrupt.if_;

            // Only pause as long as (IE & IF) != 0
            if (m_Memory.m_HaltState != Memory::HALTSTATE_NONE && interrupts != 0)
            {
                // If IntrWait only resume if requested interrupt is encountered
                if (!m_Memory.m_IntrWait || (interrupts & m_Memory.m_IntrWaitMask) != 0)
                {
                    m_Memory.m_HaltState = Memory::HALTSTATE_NONE;
                    m_Memory.m_IntrWait = false;
                }
            }

            // Raise an IRQ if neccessary
            if (m_Memory.m_Interrupt.ime && interrupts)
                m_ARM.RaiseIRQ();

            // Run the hardware components
            if (m_Memory.m_HaltState != Memory::HALTSTATE_STOP)
            {
                int forward_steps = 0;

                // Do next pending DMA transfer
                m_Memory.RunDMA();

                if (m_Memory.m_HaltState != Memory::HALTSTATE_HALT)
                {
                    m_ARM.cycles = 0;
                    m_ARM.Step();
                    forward_steps = m_ARM.cycles - 1;
                }

                for (int j = 0; j < forward_steps + 1; j++)
                {
                    m_Memory.RunTimer();

                    if (m_Memory.m_Video.m_WaitCycles == 0)
                    {
                        m_Memory.m_Video.Step();

                        if (m_Memory.m_Video.m_RenderScanline && (i / FRAME_CYCLES) == m_SpeedMultiplier - 1)
                        {
                            m_Memory.m_Video.Render();
                            m_DidRender = true;
                        }
                    }
                    else
                        m_Memory.m_Video.m_WaitCycles--;

                    if (((i + j) % 4) == 0)
                    {
                        m_Memory.m_Audio.m_QuadChannel[0].Step();
                        m_Memory.m_Audio.m_QuadChannel[1].Step();
                        m_Memory.m_Audio.m_WaveChannel.Step();
                    }

                    if (m_Memory.m_Audio.m_WaitCycles == 0)
                        m_Memory.m_Audio.Step();
                    else
                        m_Memory.m_Audio.m_WaitCycles--;
                }

                i += forward_steps;
            }
        }
    }

    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      SetKeyState
    ///
    ///////////////////////////////////////////////////////////
    void GBA::SetKeyState(Key key, bool pressed)
    {
        if (pressed)
            m_Memory.m_KeyInput &= ~(int)key;
        else
            m_Memory.m_KeyInput |= (int)key;
    }

    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      SetSpeedUp
    ///
    ///////////////////////////////////////////////////////////
    void GBA::SetSpeedUp(int multiplier)
    {
        m_SpeedMultiplier = multiplier;
    }

    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      GetVideoBuffer
    ///
    ///////////////////////////////////////////////////////////
    u32* GBA::GetVideoBuffer()
    {
        return m_Memory.m_Video.m_OutputBuffer;
    }

    ///////////////////////////////////////////////////////////
    /// \author  Frederic Meyer
    /// \date    July 31th, 2016
    /// \fn      HasRendered
    ///
    ///////////////////////////////////////////////////////////
    bool GBA::HasRendered()
    {
        return m_DidRender;
    }
}
