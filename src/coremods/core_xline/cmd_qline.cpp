/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008, 2010 Craig Edwards <brain@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"
#include "core_xline.h"

CommandQline::CommandQline(Module* parent)
	: Command(parent, "QLINE", 1, 3)
{
	flags_needed = 'o';
	syntax = "<nickmask> [<duration> :<reason>]";
}

CmdResult CommandQline::Handle(User* user, const Params& parameters)
{
	if (parameters.size() >= 3)
	{
		NickMatcher matcher;
		if (InsaneBan::MatchesEveryone(parameters[0], matcher, user, "Q", "nickmasks"))
			return CMD_FAILURE;

		if (parameters[0].find('@') != std::string::npos || parameters[0].find('!') != std::string::npos || parameters[0].find('.') != std::string::npos)
		{
			user->WriteNotice("*** A Q-line only bans a nick pattern, not a nick!user@host pattern.");
			return CMD_FAILURE;
		}

		unsigned long duration;
		if (!InspIRCd::Duration(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for Q-line.");
			return CMD_FAILURE;
		}
		QLine* ql = new QLine(ServerInstance->Time(), duration, user->nick, parameters[2], parameters[0]);
		if (ServerInstance->XLines->AddLine(ql,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s added a permanent Q-line on %s: %s", user->nick.c_str(), parameters[0].c_str(), parameters[2].c_str());
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s added a timed Q-line on %s, expires in %s (on %s): %s",
					user->nick.c_str(), parameters[0].c_str(), InspIRCd::DurationString(duration).c_str(),
					InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(), parameters[2].c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete ql;
			user->WriteNotice("*** Q-line for " + parameters[0] + " already exists.");
		}
	}
	else
	{
		std::string reason;

		if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "Q", reason, user))
		{
			ServerInstance->SNO->WriteToSnoMask('x', "%s removed Q-line on %s: %s", user->nick.c_str(), parameters[0].c_str(), reason.c_str());
		}
		else
		{
			user->WriteNotice("*** Q-line " + parameters[0] + " not found on the list.");
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}

bool CommandQline::NickMatcher::Check(User* user, const std::string& nick) const
{
	return InspIRCd::Match(user->nick, nick);
}
