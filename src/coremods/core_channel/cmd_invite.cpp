/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2008, 2010 Craig Edwards <brain@inspircd.org>
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
#include "core_channel.h"
#include "invite.h"

enum
{
	// From ircd-hybrid.
	RPL_INVITELIST = 336,
	RPL_ENDOFINVITELIST = 337
};

CommandInvite::CommandInvite(Module* parent, Invite::APIImpl& invapiimpl)
	: Command(parent, "INVITE", 0, 0)
	, invapi(invapiimpl)
{
	Penalty = 4;
	syntax = "[<nick> <channel> [<time>]]";
}

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle(User* user, const Params& parameters)
{
	ModResult MOD_RESULT;

	if (parameters.size() >= 2)
	{
		User* u;
		if (IS_LOCAL(user))
			u = ServerInstance->FindNickOnly(parameters[0]);
		else
			u = ServerInstance->FindNick(parameters[0]);

		Channel* c = ServerInstance->FindChan(parameters[1]);
		time_t timeout = 0;
		if (parameters.size() >= 3)
		{
			if (IS_LOCAL(user))
			{
				unsigned long duration;
				if (!InspIRCd::Duration(parameters[2], duration))
				{
					user->WriteNotice("*** Invalid duration for invite");
					return CMD_FAILURE;
				}
				timeout = ServerInstance->Time() + duration;
			}
			else if (parameters.size() > 3)
				timeout = ConvToNum<time_t>(parameters[3]);
		}

		if (!c)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[1]));
			return CMD_FAILURE;
		}
		if ((!u) || (u->registered != REG_ALL))
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CMD_FAILURE;
		}

		// Verify channel timestamp if the INVITE is coming from a remote server
		if (!IS_LOCAL(user))
		{
			// Remote INVITE commands must carry a channel timestamp
			if (parameters.size() < 3)
				return CMD_INVALID;

			// Drop the invite if our channel TS is lower
			time_t RemoteTS = ConvToNum<time_t>(parameters[2]);
			if (c->age < RemoteTS)
				return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, c->name, "You're not on that channel!");
			return CMD_FAILURE;
		}

		if (c->HasUser(u))
		{
			user->WriteNumeric(ERR_USERONCHANNEL, u->nick, c->name, "is already on channel");
			return CMD_FAILURE;
		}

		FIRST_MOD_RESULT(OnUserPreInvite, MOD_RESULT, (user,u,c,timeout));

		if (MOD_RESULT == MOD_RES_DENY)
		{
			return CMD_FAILURE;
		}
		else if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			if (IS_LOCAL(user))
			{
				unsigned int rank = c->GetPrefixValue(user);
				if (rank < HALFOP_VALUE)
				{
					user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(c, HALFOP_VALUE, "send an invite"));
					return CMD_FAILURE;
				}
			}
		}

		LocalUser* const localtargetuser = IS_LOCAL(u);
		if (localtargetuser)
		{
			invapi.Create(localtargetuser, c, timeout);
			ClientProtocol::Messages::Invite invitemsg(user, localtargetuser, c);
			localtargetuser->Send(ServerInstance->GetRFCEvents().invite, invitemsg);
		}

		if (IS_LOCAL(user))
		{
			user->WriteNumeric(RPL_INVITING, u->nick, c->name);
			if (u->IsAway())
				user->WriteNumeric(RPL_AWAY, u->nick, u->awaymsg);
		}

		char prefix = 0;
		unsigned int minrank = 0;
		switch (invapi.announceinvites)
		{
			case Invite::ANNOUNCE_OPS:
			{
				prefix = '@';
				minrank = OP_VALUE;
				break;
			}

			case Invite::ANNOUNCE_DYNAMIC:
			{
				PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
				if (mh)
				{
					prefix = mh->GetPrefix();
					minrank = mh->GetPrefixRank();
				}
				else
				{
					prefix = '@';
					minrank = OP_VALUE;
				}
				break;
			}

			default:
				break;
		}

		CUList excepts;
		FOREACH_MOD(OnUserInvite, (user, u, c, timeout, minrank, excepts));

		if (invapi.announceinvites != Invite::ANNOUNCE_NONE)
		{
			excepts.insert(user);
			ClientProtocol::Messages::Privmsg privmsg(ServerInstance->FakeClient, c, InspIRCd::Format("*** %s invited %s into the channel", user->nick.c_str(), u->nick.c_str()), MSG_NOTICE);
			c->Write(ServerInstance->GetRFCEvents().privmsg, privmsg, prefix, excepts);
		}
	}
	else if (IS_LOCAL(user))
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		const Invite::List* list = invapi.GetList(IS_LOCAL(user));
		if (list)
		{
			for (Invite::List::const_iterator i = list->begin(); i != list->end(); ++i)
				user->WriteNumeric(RPL_INVITELIST, (*i)->chan->name);
		}
		user->WriteNumeric(RPL_ENDOFINVITELIST, "End of INVITE list");
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandInvite::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
