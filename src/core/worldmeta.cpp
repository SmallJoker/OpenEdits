#include "worldmeta.h"
#include "packet.h"
#include "script/scriptevent.h"

WorldMeta::WorldMeta(const std::string &id) :
	IWorldMeta(id)
{
}

WorldMeta::Type WorldMeta::idToType(const std::string &id)
{
	switch (id[0]) {
		case 'P': return WorldMeta::Type::Persistent;
		case 'T': return WorldMeta::Type::TmpDraw; // or TmpSimple
		case 'I': return WorldMeta::Type::Readonly;
	}
	return WorldMeta::Type::MAX_INVALID;
}

void WorldMeta::readSpecific(Packet &pkt)
{
	ASSERT_FORCED(pkt.data_version != 0, "invalid proto ver");

	if (pkt.data_version < 9)
		return;

	u16 flags = pkt.read<u16>();
	// duplicate of Client::pkt_ActivateBlock
	keys[0].set( !!(flags & (1 << 0))); // 1.0f (active) or 0.0f (stopped)
	keys[1].set( !!(flags & (1 << 2)));
	keys[2].set( !!(flags & (1 << 2)));
	switch_state = (flags & (1 << 3));
}

void WorldMeta::writeSpecific(Packet &pkt) const
{
	ASSERT_FORCED(pkt.data_version != 0, "invalid proto ver");

	if (pkt.data_version < 9)
		return;

	u16 flags = 0
		+ (keys[0].isActive() << 0)
		+ (keys[1].isActive() << 1)
		+ (keys[2].isActive() << 2)
		+ (switch_state       << 3)
	;

	pkt.write(flags);
}



PlayerFlags WorldMeta::getPlayerFlags(const std::string &name) const
{
	auto it = m_player_flags.find(name);
	if (it != m_player_flags.end())
		return it->second;

	if (edit_code.empty()) {
		// Default permissions
		if (type == Type::TmpSimple)
			return PlayerFlags(PlayerFlags::PF_EDIT);
		if (type == Type::TmpDraw)
			return PlayerFlags(PlayerFlags::PF_EDIT_DRAW);
	}
	return PlayerFlags();
}

void WorldMeta::setPlayerFlags(const std::string &name, const PlayerFlags pf)
{
	m_player_flags[name] = pf;
}

void WorldMeta::changePlayerFlags(const std::string &name, playerflags_t changed, playerflags_t mask)
{
	PlayerFlags pf = getPlayerFlags(name);
	pf.set(changed, mask);
	m_player_flags[name] = pf;
}

void WorldMeta::readPlayerFlags(Packet &pkt)
{
	if (pkt.getRemainingBytes() == 0)
		return; // Manually created world

	u8 version = pkt.read<u8>();
	if (version < 4 || version > 5)
		throw std::runtime_error("Incompatible player flags version");

	m_player_flags.clear();

	if (version < 5)
		return;

	// Useful to enforce default flags in the future
	playerflags_t mask = pkt.read<playerflags_t>(); // known flags

	while (true) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break;

		PlayerFlags pf(0); // defaults
		playerflags_t flags = pkt.read<playerflags_t>();
		pf.flags |= flags & mask;

		m_player_flags.emplace(name, pf);
	}
}

void WorldMeta::writePlayerFlags(Packet &pkt) const
{
	pkt.write<u8>(5);
	pkt.write<playerflags_t>(PlayerFlags::PF_MASK_WORLD);

	for (auto it : m_player_flags) {
		if ((it.second.flags & PlayerFlags::PF_MASK_WORLD) == 0)
			continue;
		if (it.first == owner)
			continue;

		pkt.writeStr16(it.first);
		pkt.write<playerflags_t>(it.second.flags & PlayerFlags::PF_MASK_WORLD);
	}
	pkt.writeStr16(""); // end
}

void WorldMeta::trimChatHistory(size_t nelements)
{
	for (auto it = chat_history.begin(); it != chat_history.end();) {
		if (chat_history.size() <= nelements)
			return;

		it = chat_history.erase(it);
	}
}
