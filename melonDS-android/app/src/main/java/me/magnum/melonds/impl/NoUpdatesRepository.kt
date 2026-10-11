package me.magnum.melonds.impl

import me.magnum.melonds.domain.model.appupdate.AppUpdate
import me.magnum.melonds.domain.repositories.UpdatesRepository

/**
 * Punica has no update check: a new version is installed from the APK the CI builds. (melonDS's update
 * check looked for melonDS releases, and its Play Store version none; both versions were removed.)
 */
class NoUpdatesRepository : UpdatesRepository {
    override suspend fun checkNewUpdate(): Result<AppUpdate?> {
        return Result.success(null)
    }

    override fun skipUpdate(update: AppUpdate) {
        // Do nothing
    }

    override fun notifyUpdateDownloaded(update: AppUpdate) {
        // Do nothing
    }
}