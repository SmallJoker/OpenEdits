#include "server.h"
#include "core/auth.h"
#include "core/packet.h"
#include "core/player.h"
#include "core/utils.h" // get_next_part
#include "core/world.h"
#include "server/database_auth.h"
#include "server/database_world.h"
#include "server/eeo_converter.h"
#include "version.h"


void Server::registerChatCommands()
{
	m_chatcmd.add("/help", (ChatCommandAction)&Server::chat_Help);
	m_chatcmd.add("/respawn", (ChatCommandAction)&Server::chat_Respawn);
	m_chatcmd.add("/teleport", (ChatCommandAction)&Server::chat_Teleport);

	// Permissions
	m_chatcmd.add("/setpass", (ChatCommandAction)&Server::chat_SetPass);
	m_chatcmd.add("/setcode", (ChatCommandAction)&Server::chat_SetCode);
	m_chatcmd.add("/code", (ChatCommandAction)&Server::chat_Code);
	m_chatcmd.add("/ban", (ChatCommandAction)&Server::chat_Ban);
	m_chatcmd.add("/flags", (ChatCommandAction)&Server::chat_Flags);
	m_chatcmd.add("/ffilter", (ChatCommandAction)&Server::chat_FFilter);
	m_chatcmd.add("/fset", (ChatCommandAction)&Server::chat_FSet);
	m_chatcmd.add("/fdel", (ChatCommandAction)&Server::chat_FDel);

	// World control
	m_chatcmd.add("/clear", (ChatCommandAction)&Server::chat_Clear);
	m_chatcmd.add("/import", (ChatCommandAction)&Server::chat_Import);
	m_chatcmd.add("/load", (ChatCommandAction)&Server::chat_Load);
	m_chatcmd.add("/save", (ChatCommandAction)&Server::chat_Save);
	m_chatcmd.add("/title", (ChatCommandAction)&Server::chat_Title);
}

// ----------- Server checks -----------

bool Server::checkSize(std::string &out, blockpos_t size)
{
	if ((size.X < 3 || size.X > 300)
			|| (size.Y < 3 || size.Y > 300)) {
		out = "Width and height must be in the range [3,300]";
		return false;
	}
	return true;
}

bool Server::checkTitle(std::string &out, std::string &title)
{
	title = strtrim(title);
	if (title.empty()) {
		title = generate_world_title();
		return true;
	}

	// TODO: This does not take unicode well into account
	if (title.size() > 40) {
		out = "Title too long. Limiting to 40 characters.";
		title.resize(40);
	}

	bool have_unprintable = false;
	for (char &c : title) {
		if (!std::isprint(c)) {
			c = '?';
			have_unprintable = true;
		}
	}
	if (have_unprintable)
		out = "Substituted non-ASCII characters in the title.";

	return true;
}

// -------------- Chat commands -------------

void Server::systemChatSend(Player *player, const std::string &msg, bool broadcast)
{
	Packet pkt;
	pkt.write(Packet2Client::Chat);
	pkt.write<peer_t>(0);
	pkt.writeStr16(msg);
	m_con->send(player->peer_id, 0, pkt);
}

Player *Server::findPlayer(const World *world, std::string name)
{
	for (char &c : name)
		c = toupper(c);

	for (auto p : m_players) {
		if (p.second->getWorld().get() != world)
			continue;
		if (p.second->name != name)
			continue;

		return p.second;
	}
	return nullptr;
}


CHATCMD_FUNC(Server::chat_Help)
{
	std::string cmd(get_next_part(msg));
	if (cmd.empty()) {
		systemChatSend(player, "Use '/help CMD' for details. Available commands: " + m_chatcmd.dumpUI());
		return;
	}

	static const struct {
		const std::string cmd;
		const std::string text;
	} help_LUT[] = {
		{ "respawn", "Respawns you" },
		{ "teleport", "Syntax: /teleport [SRC] DST.\nTeleports players. DST can be of the format 'X,Y'." },
		// Permissions
		{ "setpass", "Syntax: /flags [PLAYERNAME] PASSWORD PASSWORD" },
		{ "setcode", "Syntax: /setcode [-f] [WORLDCODE]\nChanges the world code or disables it. "
			"-f (optional) removes all player's temporary edit and god mode flags." },
		{ "code", "Syntax: /code WORLDCODE\nGrants edit access." },
		{ "ban", "Syntax: /ban PLAYERNAME MINUTES [REASON TEXT]\nBans the specified player." },
		{ "flags", "Syntax: /flags [PLAYERNAME]\nLists the provided (or own) player's flags." },
		{ "ffilter", "Syntax: //filter FLAG1 [...]\nLists all players matching the flag filter." },
		{ "fset", "Syntax: /fset PLAYERNAME FLAG1 [...]\nSets one or more flags for a player." },
		{ "fdel", "The complement of /fset" },
		// World control
		{ "clear", "Syntax: /clear [W] [H]\nW,H: integer (optional) to specify the new world dimensions." },
		{ "import", "Syntax: /import FILENAME\nFILENAME: .eelvl format without the file extension" },
		{ "save", "Saves the world blocks, meta and player flags." },
		{ "title", "Syntax: /title TITLE\nChanges the world's title (not saved)" },
		// Other
		{ "version", std::string("Server version: ") + VERSION_STRING },
	};

	// Trim "/" if necessary
	if (cmd[0] == '/')
		cmd = cmd.substr(1);

	for (auto v : help_LUT) {
		if (cmd != v.cmd)
			continue;

		std::string answer = v.text;

		auto main = m_chatcmd.get("/" + v.cmd);
		if (main) {
			std::string subcmds = main->dumpUI();
			if (!subcmds.empty()) {
				answer.append("\nSubcommands: ");
				answer.append(subcmds);
			}
		}
		systemChatSend(player, answer);
		return;
	}

	systemChatSend(player, "No help available for command " + cmd);
}

CHATCMD_FUNC(Server::chat_SetPass)
{
	// [who] pass pass
	std::string newpass(get_next_part(msg));
	std::string confirm(get_next_part(msg));
	std::string who(get_next_part(msg));

	if (!who.empty()) {
		std::swap(newpass, confirm); // pass who pass
		std::swap(confirm, who); // who pass pass
	} else {
		who = player->name;
	}

	for (char &c : who)
		c = toupper(c);

	if (!m_auth_db) {
		systemChatSend(player, "Auth database is dead.");
		return;
	}

	AuthAccount info;
	if (!m_auth_db->load(player->name, &info)) {
		systemChatSend(player, "Your account is not registered.");
		return;
	}

	if (confirm != newpass) {
		systemChatSend(player, "Passwords do not match.");
		return;
	}

	if (who == player->name) {
		for (auto p : m_players) {
			if (p.second->name != player->name || p.second == player)
				continue;

			systemChatSend(player, "For safety reasons please leave all other worlds first.");
			return;
		}
	} else {
		// Auth check
		AuthAccount info_who;
		if (!m_auth_db->load(who, &info_who)
				|| info.level < AuthAccount::AL_MODERATOR
				|| info.level < info_who.level) {
			systemChatSend(player, "Insufficient privileges or the specified account is not registered.");
			return;
		}
	}

	Auth auth;
	auth.hash(m_auth_db->getUniqueSalt(), newpass);
	bool ok = m_auth_db->setPassword(who, auth.output);

	if (ok)
		systemChatSend(player, "Changed password of " + who + " to: " + newpass);
	else
		systemChatSend(player, "Internal error");
}

CHATCMD_FUNC(Server::chat_SetCode)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string flag(get_next_part(msg));
	std::string code(get_next_part(msg));

	if (flag[0] != '-') {
		code = flag;
		flag.clear();
	}

	bool do_change_flags = (flag == "-f");

	auto world = player->getWorld();
	world->getMeta().edit_code = code;

	if (code.empty()) {
		systemChatSend(player, "Disabled access via code.");
	} else {
		systemChatSend(player, "Changed code.");
	}

	if (!do_change_flags)
		return;

	constexpr playerflags_t to_revoke = PlayerFlags::PF_EDIT_DRAW | PlayerFlags::PF_GODMODE;
	auto try_revoke = [&] (PlayerFlags &pf) {
		if (pf.flags & PlayerFlags::PF_MASK_WORLD)
			return false;

		bool changed = pf.flags & to_revoke;
		pf.set(0, to_revoke);
		return changed;
	};

	for (auto it : m_players) {
		if (it.second->getWorld() != world)
			continue;

		auto pf = it.second->getFlags();
		if (try_revoke(pf)) {
			world->getMeta().setPlayerFlags(it.second->name, pf);
			handlePlayerFlagsChange(it.second, to_revoke);
		}
	}
}

CHATCMD_FUNC(Server::chat_Code)
{
	auto &meta = player->getWorld()->getMeta();
	if (meta.edit_code.empty()) {
		systemChatSend(player, "There is no code specified.");
		return;
	}

	std::string code(strtrim(msg));
	if (code != meta.edit_code) {
		systemChatSend(player, "Incorrect code.");
		return;
	}

	auto old_pf = player->getFlags();
	PlayerFlags pf;
	switch (meta.type) {
		case WorldMeta::Type::TmpSimple:
			pf.flags = PlayerFlags::PF_EDIT;
			break;
		case WorldMeta::Type::TmpDraw:
			pf.flags = PlayerFlags::PF_EDIT_DRAW;
			break;
		case WorldMeta::Type::Persistent:
			pf.flags = PlayerFlags::PF_EDIT_DRAW
				| PlayerFlags::PF_GODMODE;
			break;
		default:
			systemChatSend(player, "Internal error: invalid world type");
			return;
	}

	old_pf.flags |= pf.flags;
	meta.setPlayerFlags(player->name, old_pf);

	{
		Packet out;
		out.write(Packet2Client::PlayerFlags);
		out.write<playerflags_t>(pf.flags); // new flags
		out.write<playerflags_t>(pf.flags); // mask
		m_con->send(player->peer_id, 1, out);
	}
}

CHATCMD_FUNC(Server::chat_Ban)
{
	const PlayerFlags myflags = player->getFlags();
	if (!myflags.check(PlayerFlags::PF_COOWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	if (!m_auth_db) {
		systemChatSend(player, "Auth database is dead.");
		return;
	}


	std::string who(get_next_part(msg));
	Player *target = findPlayer(player->getWorld().get(), who);
	if (!target) {
		systemChatSend(player, "Cannot find player '" + who + "'.");
		return;
	}

	AuthBanEntry entry;
	entry.affected = target->name;
	entry.context = player->getWorld()->getMeta().id;
	if (m_auth_db->getBanRecord(entry.affected, entry.context, &entry)) {
		// Why are they still here?
		systemChatSend(target, "Ban circumvention??");
		m_con->disconnect(target->peer_id);
		systemChatSend(player, "Oops. They're gone now.");
		return;
	}

	if (!mayManipulatePlayer(player, target)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string minutes_s(get_next_part(msg));
	int64_t minutes_i = 0;
	string2int64(minutes_s.c_str(), &minutes_i);
	if (minutes_i <= 0) {
		systemChatSend(player, "Invalid duration specified.");
		return;
	}

	minutes_i = std::min<int64_t>(minutes_i, 60 * 2);
	entry.expiry = time(nullptr) + minutes_i * 60LL;
	entry.comment = msg;
	if (!m_auth_db->ban(entry)) {
		systemChatSend(player, "Internal error: failed to ban.");
		return;
	}

	std::string time = " (" + std::to_string(minutes_i) +  " minute(s)).";
	sendMsg(target->peer_id, player->name + " banned you from this world" + time);
	systemChatSend(player, player->name + " banned " + target->name + time);

	Packet dummy;
	pkt_Leave(target->peer_id, dummy);
}

CHATCMD_FUNC(Server::chat_Flags)
{
	std::string who(get_next_part(msg));
	if (who.empty())
		who = player->name;
	else
		for (char &c : who)
			c = toupper(c);

	std::string ret;
	ret.append("Flags of player ");
	ret.append(who);
	ret.append(": ");
	// oh my fucking god
	ret.append(player->getWorld()->getMeta().getPlayerFlags(who).toHumanReadable());

	systemChatSend(player, ret);
}

CHATCMD_FUNC(Server::chat_FFilter)
{
	PlayerFlags specified(0);
	std::string flag_string(get_next_part(msg));
	while (!flag_string.empty()) {
		playerflags_t flags_new;
		if (!PlayerFlags::stringToPlayerFlags(flag_string, &flags_new)) {
			systemChatSend(player, "Unknown flag: " + flag_string +
				". Available flags: " + PlayerFlags::getFlagList());
			return;
		}
		specified.flags |= flags_new;

		flag_string = get_next_part(msg);
	}

	const auto &meta = player->getWorld()->getMeta();

	// List all players with flags
	std::string output("List of players with flags ");
	output.append(specified.toHumanReadable());
	output.append(":");

	for (auto it : meta.getAllPlayerFlags()) {
		if ((it.second.flags | specified.flags) == 0)
			continue;
		output.append(" " + it.first);
	}

	systemChatSend(player, output);
}

playerflags_t Server::mayManipulatePlayer(Player *actor, Player *target)
{
	if (!actor || !target)
		return 0;

	PlayerFlags flags_a = actor->getFlags();
	PlayerFlags flags_t = target->getFlags();

	constexpr playerflags_t scope = PlayerFlags::PF_MASK_WORLD;

	// "actor" must have more world-specific bits set than "target"
	if (flags_a.flags & ~flags_t.flags & scope) {
		// Allow flag changes
		if (flags_a.check(PlayerFlags::PF_OWNER))
			return PlayerFlags::PF_CNG_MASK_OWNER;
		else if (flags_a.check(PlayerFlags::PF_COOWNER))
			return PlayerFlags::PF_CNG_MASK_COOWNER;
		else if (flags_a.check(PlayerFlags::PF_COLLAB))
			return 0;

		fprintf(stderr, "Unhandled flags %08X for player %s\n", flags_a.flags, actor->name.c_str());
		return 0;
	}
	return 0;
}


bool Server::changePlayerFlags(Player *player, std::string msg, bool do_add)
{
	const PlayerFlags myflags = player->getFlags();
	if (!myflags.check(PlayerFlags::PF_COOWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return false;
	}

	auto world = player->getWorld();
	auto &meta = world->getMeta();

	std::string playername(get_next_part(msg));
	for (char &c : playername)
		c = toupper(c);

	// Search for existing records
	Player *target_player = findPlayer(world.get(), playername);
	PlayerFlags targetflags = meta.getPlayerFlags(playername);
	const playerflags_t old_flags = targetflags.flags;


	if (!target_player && targetflags.flags == 0) {
		systemChatSend(player, "Cannot find player " + playername);
		return false;
	}

	// Flags that current player is allowed to change
	playerflags_t allowed_to_change = mayManipulatePlayer(player, target_player);

	// Read in all specified flags
	playerflags_t flags_specified = 0;
	std::string flag_string(get_next_part(msg));
	while (!flag_string.empty()) {
		playerflags_t flags_new;
		if (!PlayerFlags::stringToPlayerFlags(flag_string, &flags_new)) {
			systemChatSend(player, "Unknown flag: " + flag_string +
				". Available flags: " + PlayerFlags::getFlagList());
			return false;
		}
		flags_specified |= flags_new;

		flag_string = get_next_part(msg);
	}

	if ((flags_specified & ~allowed_to_change) != 0) {
		systemChatSend(player, "Insufficient permissions");
		return false;
	}

	// Perform the operation
	if (do_add) {
		targetflags.set(flags_specified, flags_specified);
	} else {
		targetflags.set(0, flags_specified);
	}

	meta.setPlayerFlags(playername, targetflags);
	// Get up-to-date information
	targetflags = meta.getPlayerFlags(playername);

	if (targetflags.flags != old_flags)
		handlePlayerFlagsChange(target_player, flags_specified);

	chat_Flags(player, playername);
	return true;
}

void Server::handlePlayerFlagsChange(Player *player, playerflags_t flags_mask)
{
	if (!player)
		return;

	PlayerFlags flags = player->getFlags();

	// Notify about new flags
	{
		Packet out;
		out.write(Packet2Client::PlayerFlags);
		out.write<playerflags_t>(flags.flags & flags_mask); // new flags
		out.write<playerflags_t>(flags_mask); // mask
		m_con->send(player->peer_id, 1, out);
	}

	// Remove god mode if active
	if (player->godmode && !(flags.flags & PlayerFlags::PF_GODMODE)) {
		player->setGodMode(false);

		Packet out;
		out.write(Packet2Client::GodMode);
		out.write(player->peer_id);
		out.write<u8>(player->godmode);

		broadcastInWorld(player, 1, out);
	}
}


CHATCMD_FUNC(Server::chat_FSet)
{
	changePlayerFlags(player, msg, true);
}

CHATCMD_FUNC(Server::chat_FDel)
{
	changePlayerFlags(player, msg, false);
}

CHATCMD_FUNC(Server::chat_Respawn)
{
	respawnPlayer(player, true);
}

CHATCMD_FUNC(Server::chat_Teleport)
{
	if (!player->getFlags().check(PlayerFlags::PF_GODMODE)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string src_str(get_next_part(msg));
	std::string dst_str(get_next_part(msg));

	Player *src = nullptr;
	core::vector2df dst;

	if (dst_str.empty()) {
		// teleport DESTINATION
		src = player;
		dst_str = src_str;
	} else {
		// teleport SRC DST
		src = findPlayer(player->getWorld().get(), src_str);
	}

	auto parts = strsplit(dst_str, ',');
	if (parts.size() == 2) {
		// X,Y
		int64_t x = -1;
		int64_t y = -1;
		string2int64(parts[0].c_str(), &x);
		string2int64(parts[1].c_str(), &y);

		dst = core::vector2df(x, y);
	} else {
		// Player name
		Player *player_dst = findPlayer(player->getWorld().get(), dst_str);
		if (!player_dst) {
			systemChatSend(player, "Destination player not found");
			return;
		}
		dst = player_dst->pos;
	}

	if (src == player) {
		if (!player->getFlags().check(PlayerFlags::PF_COOWNER)) {
			systemChatSend(player, "Insufficient permissions");
			return;
		}
	}

	if (!player->getWorld()->isValidPosition(dst.X, dst.Y)) {
		systemChatSend(player, "Invalid destination position");
		return;
	}

	teleportPlayer(src, dst, false);
}

/// cmd: /clear [width] [height]
CHATCMD_FUNC(Server::chat_Clear)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string width_s(get_next_part(msg));
	std::string height_s(get_next_part(msg));

	auto old_world = player->getWorld();
	int64_t width_i = -1,
		height_i = -1;

	if (width_s.empty()) {
		width_i = old_world->getSize().X;
		height_i = old_world->getSize().Y;
	} else if (string2int64(width_s.c_str(), &width_i)) {
		if (height_s.empty())
			height_i = width_i;
		else
			string2int64(height_s.c_str(), &height_i);
	}

	blockpos_t size(
		std::min<u16>(width_i,  UINT16_MAX),
		std::min<u16>(height_i, UINT16_MAX)
	);
	std::string err_msg;

	if (!checkSize(err_msg, size)) {
		systemChatSend(player, err_msg);
		return;
	}

	auto world = std::make_shared<World>(old_world.get());

	try {
		world->createEmpty(size);
	} catch (std::runtime_error &e) {
		systemChatSend(player, std::string("ERROR: ") + e.what());
		return;
	}

	Packet out;
	writeWorldData(out, *world.get(), true);

	for (auto it : m_players) {
		if (it.second->getWorld() == old_world) {
			it.second->setWorld(world);
			m_con->send(it.first, 0, out);
		}
	}

	systemChatSend(player, "Cleared!");
}

CHATCMD_FUNC(Server::chat_Import)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string filename(strtrim(msg));
	const std::string suffix = ".eelvl";
	if (!std::equal(suffix.rbegin(), suffix.rend(), filename.rbegin()))
		filename.append(suffix);

	if (filename.find('/') != std::string::npos
			|| filename.find('\\') != std::string::npos) {
		systemChatSend(player, "The file must be located next to the server executable.");
		return;
	}

	auto old_world = player->getWorld();

	auto world = std::make_shared<World>(old_world.get());
	EEOconverter conv(*world.get());

	try {
		auto old_owner = old_world->getMeta().owner;
		conv.fromFile(filename);
		world->getMeta().owner = old_owner;
	} catch (std::runtime_error &e) {
		systemChatSend(player, std::string("ERROR: ") + e.what());
		return;
	}

	systemChatSend(player, "Imported!");

	Packet out;
	writeWorldData(out, *world.get(), false);

	for (auto it : m_players) {
		if (it.second->getWorld() == old_world) {
			it.second->setWorld(world);
			m_con->send(it.first, 0, out);
			respawnPlayer(it.second, true);
		}
	}
}

CHATCMD_FUNC(Server::chat_Load)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	auto old_world = player->getWorld();
	auto world = loadWorldNoLock(old_world->getMeta().id);
	if (!world) {
		systemChatSend(player, "Failed to load world from database");
		return;
	}

	Packet pkt_world;
	writeWorldData(pkt_world, *world.get(), false);

	for (auto it : m_players) {
		if (it.second->getWorld() == old_world) {
			it.second->setWorld(world);
			m_con->send(it.first, 0, pkt_world);
		}
	}

	Packet pkt_meta;
	pkt_meta.write(Packet2Client::WorldMeta);
	world->getMeta().writeCommon(pkt_meta);
	broadcastInWorld(player, 1, pkt_meta);

	// TODO: this is inefficient
	for (auto it : m_players) {
		if (it.second->getWorld() == world) {
			respawnPlayer(it.second, true);
		}
	}
}

CHATCMD_FUNC(Server::chat_Save)
{
	if (!m_world_db)
		return;

	auto world = player->getWorld();

	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	// Slight delay for anti-spam
	if (m_auth_db) {
		if (m_auth_db->getBanRecord(world->getMeta().id, "world.save", nullptr)) {
			systemChatSend(player, "Please wait a moment before saving again.");
			return;
		}

		AuthBanEntry entry;
		entry.affected = world->getMeta().id;
		entry.context = "world.save";
		entry.expiry = time(nullptr) + 10;
		m_auth_db->ban(entry);
	}

	m_world_db->save(world.get());

	systemChatSend(player, "Saved!");
}

CHATCMD_FUNC(Server::chat_Title)
{
	if (!m_world_db)
		return;

	auto world = player->getWorld();

	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string title(msg);
	std::string err_msg;
	if (!checkTitle(err_msg, title)) {
		systemChatSend(player, err_msg);
		return;
	}
	if (!err_msg.empty())
		systemChatSend(player, err_msg);

	world->getMeta().title = title;

	Packet out;
	out.write(Packet2Client::WorldMeta);
	world->getMeta().writeCommon(out);
	broadcastInWorld(player, 1, out);
}
