/***************************************************************************
 * ROM Properties Page shell extension. (librpbase)                        *
 * IconAnimHelper.cpp: Icon animation helper.                              *
 *                                                                         *
 * Copyright (c) 2016-2019 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "IconAnimHelper.hpp"
#include "rp_image.hpp"

// C includes. (C++ namespace)
#include <cassert>

namespace LibRpBase {

/**
 * Reset the animation.
 */
void IconAnimHelper::reset(void)
{
	if (m_iconAnimData) {
		assert(m_iconAnimData->count > 1);
		assert(m_iconAnimData->count <= (int)m_iconAnimData->frames.size());
		assert(m_iconAnimData->seq_count > 1);
		assert(m_iconAnimData->seq_count <= (int)m_iconAnimData->seq_index.size());
		m_seq_idx = 0;
		m_frame = m_iconAnimData->seq_index[0];
		m_delay = m_iconAnimData->delays[0].ms;
		m_last_valid_frame = m_frame;
	} else {
		// No animation.
		m_seq_idx = 0;
		m_frame = 0;
		m_delay = 0;
		m_last_valid_frame = 0;
	}
}

/**
 * Advance the animation by one frame.
 * @param pDelay	[out] Pointer to int to store the frame delay, in milliseconds.
 * @return Next frame number. (Returns 0 if there's no animation.)
 */
int IconAnimHelper::nextFrame(int *pDelay)
{
	if (!m_iconAnimData) {
		// No animation data.
		return 0;
	}

	// Go to the next frame in the sequence.
	if (m_seq_idx >= (m_iconAnimData->seq_count - 1)) {
		// Last frame in the sequence.
		m_seq_idx = 0;
	} else {
		// Go to the next frame in the sequence.
		m_seq_idx++;
	}

	// Get the frame number associated with this sequence index.
	m_frame = m_iconAnimData->seq_index[m_seq_idx];
	assert(m_frame >= 0);
	assert(m_frame < (int)m_iconAnimData->frames.size());

	// Get the frame delay. (TODO: Must be > 0?)
	m_delay = m_iconAnimData->delays[m_seq_idx].ms;
	if (pDelay) {
		*pDelay = m_delay;
	}

	// Check if this frame is valid.
	if (m_iconAnimData->frames[m_frame] != nullptr &&
	    m_iconAnimData->frames[m_frame]->isValid())
	{
		// Frame is valid.
		m_last_valid_frame = m_frame;
	}

	return m_last_valid_frame;
}

}
