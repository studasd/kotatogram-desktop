/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class UserData;
class ChannelData;

namespace Data {

class GroupCall final {
public:
	GroupCall(not_null<ChannelData*> channel, uint64 id, uint64 accessHash);
	~GroupCall();

	[[nodiscard]] uint64 id() const;
	[[nodiscard]] not_null<ChannelData*> channel() const;
	[[nodiscard]] MTPInputGroupCall input() const;

	struct Participant {
		not_null<UserData*> user;
		TimeId date = 0;
		TimeId lastActive = 0;
		uint32 source = 0;
		bool speaking = false;
		bool muted = false;
		bool canSelfUnmute = false;
	};
	struct ParticipantUpdate {
		std::optional<Participant> was;
		std::optional<Participant> now;
	};

	[[nodiscard]] auto participants() const
		-> const std::vector<Participant> &;
	void requestParticipants();
	[[nodiscard]] bool participantsLoaded() const;
	[[nodiscard]] UserData *userBySource(uint32 source) const;

	[[nodiscard]] rpl::producer<> participantsSliceAdded();
	[[nodiscard]] rpl::producer<ParticipantUpdate> participantUpdated() const;

	void applyUpdate(const MTPGroupCall &update);
	void applyUpdate(const MTPDupdateGroupCallParticipants &update);
	void applyUpdateChecked(
		const MTPDupdateGroupCallParticipants &update);
	void applyLastSpoke(uint32 source, crl::time when, crl::time now);

	[[nodiscard]] int fullCount() const;
	[[nodiscard]] rpl::producer<int> fullCountValue() const;

	void reload();

	void setJoinMutedLocally(bool muted);
	[[nodiscard]] bool joinMuted() const;
	[[nodiscard]] bool canChangeJoinMuted() const;

private:
	void applyCall(const MTPGroupCall &call, bool force);
	void applyParticipantsSlice(
		const QVector<MTPGroupCallParticipant> &list,
		bool sendIndividualUpdates = false);
	void applyParticipantsMutes(
		const MTPDupdateGroupCallParticipants &update);

	const not_null<ChannelData*> _channel;
	const uint64 _id = 0;
	const uint64 _accessHash = 0;

	int _version = 0;
	mtpRequestId _participantsRequestId = 0;
	mtpRequestId _reloadRequestId = 0;

	std::vector<Participant> _participants;
	base::flat_map<uint32, not_null<UserData*>> _userBySource;
	QString _nextOffset;
	rpl::variable<int> _fullCount = 0;

	rpl::event_stream<ParticipantUpdate> _participantUpdates;
	rpl::event_stream<> _participantsSliceAdded;

	bool _joinMuted = false;
	bool _canChangeJoinMuted = true;
	bool _allReceived = false;

};

} // namespace Data
