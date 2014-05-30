//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2014 konstin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_RANDOM_GP_INFO_DIALOG_HPP
#define HEADER_RANDOM_GP_INFO_DIALOG_HPP

#include "states_screens/dialogs/gp_info_dialog.hpp"

#include <string>

class RandomGPInfoDialog : public GPInfoDialog
{
private:
    unsigned int m_number_of_tracks;
    std::string m_trackgroup;
    bool m_use_reverse;

public:
    static const int SPINNER_HEIGHT = 40;

    RandomGPInfoDialog();
    ~RandomGPInfoDialog() { delete m_gp; }

    /** Adds a SpinnerWidgets to choose the track groups and one to choose the
     * number of tracks */
    void addSpinners();

    GUIEngine::EventPropagation processEvent(const std::string& eventSource);
    /** Constructs a new gp from the members */
    void updateGP();
};

#endif
