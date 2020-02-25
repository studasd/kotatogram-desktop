/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/changelogs.h"

#include "storage/localstorage.h"
#include "lang/lang_keys.h"
#include "data/data_session.h"
#include "mainwindow.h"
#include "apiwrap.h"

namespace Core {
namespace {

std::map<int, const char*> BetaLogs() {
	return {
	{
		1008005,
		"\xE2\x80\xA2 Create new themes based on your color "
		"and wallpaper choices.\n"

		"\xE2\x80\xA2 Share your themes with other users via links.\n"

		"\xE2\x80\xA2 Update your theme for all its users "
		"when you change something.\n"
	},
	{
		1009000,
		"\xE2\x80\xA2 System spell checker on Windows 8+ and macOS 10.12+.\n"
	},
	{
		1009002,
		"\xE2\x80\xA2 Videos in chats start playing automatically.\n"

		"\xE2\x80\xA2 Resume playback from where you left off "
		"when watching long videos.\n"

		"\xE2\x80\xA2 Control videos, GIFs and round video messages "
		"automatic playback in "
		"Settings > Advanced > Automatic media download.\n"

		"\xE2\x80\xA2 Spell checker on Linux using Enchant.\n"
	},
	{
		1009010,
		"\xE2\x80\xA2 Switch to the Picture-in-Picture mode "
		"to watch your video in a small window.\n"

		"\xE2\x80\xA2 Change video playback speed "
		"in the playback controls '...' menu.\n"

		"\xE2\x80\xA2 Rotate photos and videos in the media viewer "
		"using the rotate button in the bottom right corner.\n"
	},
	{
		1009015,
		"\xE2\x80\xA2 Mark new messages as read "
		"while scrolling down through them.\n"

		"\xE2\x80\xA2 Bug fixes and other minor improvements."
	},

	{
		1009017,
		"\xE2\x80\xA2 Spell checker on Windows 7.\n"

		"\xE2\x80\xA2 Bug fixes and other minor improvements."
	}
	};
};

QString FormatVersionDisplay(int version) {
	return QString::number(version / 1000000)
		+ '.' + QString::number((version % 1000000) / 1000)
		+ ((version % 1000)
			? ('.' + QString::number(version % 1000))
			: QString());
}

QString FormatVersionPrecise(int version) {
	return QString::number(version / 1000000)
		+ '.' + QString::number((version % 1000000) / 1000)
		+ '.' + QString::number(version % 1000);
}

} // namespace

Changelogs::Changelogs(not_null<Main::Session*> session, int oldVersion, int oldKotatoVersion)
: _session(session)
, _oldVersion(oldVersion)
, _oldKotatoVersion(oldKotatoVersion) {

	LOG(("Previous Kotatogram version: %1").arg(_oldKotatoVersion));

	_session->data().chatsListChanges(
	) | rpl::filter([](Data::Folder *folder) {
		return !folder;
	}) | rpl::start_with_next([=] {
		addKotatoLogs();
	}, _chatsSubscription);
}

std::unique_ptr<Changelogs> Changelogs::Create(
		not_null<Main::Session*> session) {
	const auto oldVersion = Local::oldMapVersion();
	const auto oldKotatoVersion = Local::oldKotatoVersion();
	return (!cKotatoFirstRun() && oldKotatoVersion < AppKotatoVersion)
		? std::make_unique<Changelogs>(session, oldVersion, oldKotatoVersion)
		: nullptr;
}

void Changelogs::addKotatoLogs() {
	_chatsSubscription.destroy();
	
	auto baseLang = Lang::Current().baseId();
	auto currentLang = Lang::Current().id();
	QString channelLink;

	for (const auto language : { "ru", "uk", "be" }) {
		if (baseLang.startsWith(QLatin1String(language)) || currentLang == QString(language)) {
			channelLink = "https://t.me/kotatogram_ru";
			break;
		}
	}

	if (channelLink.isEmpty()) {
		channelLink = "https://t.me/kotatogram";
	}

	const auto text = tr::ktg_new_version(
		tr::now,
		lt_version,
		QString::fromLatin1(AppKotatoVersionStr),
		lt_td_version,
		QString::fromLatin1(AppVersionStr),
		lt_link,
		channelLink);
	addLocalLog(text.trimmed());
}

void Changelogs::requestCloudLogs() {
	_chatsSubscription.destroy();

	const auto callback = [this](const MTPUpdates &result) {
		_session->api().applyUpdates(result);

		auto resultEmpty = true;
		switch (result.type()) {
		case mtpc_updateShortMessage:
		case mtpc_updateShortChatMessage:
		case mtpc_updateShort:
			resultEmpty = false;
			break;
		case mtpc_updatesCombined:
			resultEmpty = result.c_updatesCombined().vupdates().v.isEmpty();
			break;
		case mtpc_updates:
			resultEmpty = result.c_updates().vupdates().v.isEmpty();
			break;
		case mtpc_updatesTooLong:
		case mtpc_updateShortSentMessage:
			LOG(("API Error: Bad updates type in app changelog."));
			break;
		}
		if (resultEmpty) {
			addLocalLogs();
		}
	};
	_session->api().requestChangelog(
		FormatVersionPrecise(_oldVersion),
		crl::guard(this, callback));
}

void Changelogs::addLocalLogs() {
	if (AppBetaVersion || cAlphaVersion()) {
		addBetaLogs();
	}
	if (!_addedSomeLocal) {
		const auto text = tr::lng_new_version_wrap(
			tr::now,
			lt_version,
			QString::fromLatin1(AppVersionStr),
			lt_changes,
			tr::lng_new_version_minor(tr::now),
			lt_link,
			qsl("https://desktop.telegram.org/changelog"));
		addLocalLog(text.trimmed());
	}
}

void Changelogs::addLocalLog(const QString &text) {
	auto textWithEntities = TextWithEntities{ text };
	TextUtilities::ParseEntities(textWithEntities, TextParseLinks);
	_session->data().serviceNotification(textWithEntities);
	_addedSomeLocal = true;
};

void Changelogs::addBetaLogs() {
	for (const auto [version, changes] : BetaLogs()) {
		addBetaLog(version, changes);
	}
}

void Changelogs::addBetaLog(int changeVersion, const char *changes) {
	if (_oldVersion >= changeVersion) {
		return;
	}
	const auto version = FormatVersionDisplay(changeVersion);
	const auto text = qsl("New in version %1:\n\n").arg(version)
		+ QString::fromUtf8(changes).trimmed();
	addLocalLog(text);
}

} // namespace Core
